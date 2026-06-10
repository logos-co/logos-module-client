{
  description = "Logos Module Client - Proxy API and SDK client library";

  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    logos-cpp-sdk.inputs.logos-protocol.follows = "logos-protocol";
    logos-protocol = {
      url = "github:logos-co/logos-protocol";
      inputs.logos-nix.follows = "logos-nix";
    };
    logos-qt-sdk = {
      url = "github:logos-co/logos-qt-sdk";
      inputs.logos-nix.follows = "logos-nix";
      inputs.logos-protocol.follows = "logos-protocol";
      inputs.logos-cpp-sdk.follows = "logos-cpp-sdk";
    };
  };

  outputs = { self, nixpkgs, logos-nix, logos-cpp-sdk, logos-protocol, logos-qt-sdk }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
        logosSdk = logos-cpp-sdk.packages.${system}.default;
        logosProtocolPkg = logos-protocol.packages.${system}.default;
        logosQtSdk = logos-qt-sdk.packages.${system}.default;
      });
    in
    {
      packages = forAllSystems ({ pkgs, system, logosSdk, logosProtocolPkg, logosQtSdk }:
        let
          common = import ./nix/default.nix { inherit pkgs logosSdk logosProtocolPkg logosQtSdk; };
          src = ./.;

          build = import ./nix/build.nix { inherit pkgs common src; };
          lib = import ./nix/lib.nix { inherit pkgs common build; };
          include = import ./nix/include.nix { inherit pkgs common src; };
          tests = import ./nix/tests.nix { inherit pkgs common build; };

          logos-module-client = pkgs.symlinkJoin {
            name = "logos-module-client";
            paths = [ lib include ];
          };
        in
        {
          logos-module-client-lib = lib;
          logos-module-client-include = include;
          logos-module-client-tests = tests;
          logos-module-client = logos-module-client;
          default = logos-module-client;
        }
      );

      checks = forAllSystems ({ pkgs, system, ... }:
        let
          testsPkg = self.packages.${system}.logos-module-client-tests;
        in {
          tests = pkgs.runCommand "logos-module-client-tests" {
            nativeBuildInputs = [ testsPkg ] ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.qt6.qtbase ];
          } ''
            export QT_QPA_PLATFORM=offscreen
            ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
              export QT_PLUGIN_PATH="${pkgs.qt6.qtbase}/${pkgs.qt6.qtbase.qtPluginPrefix}"
            ''}
            mkdir -p $out
            echo "Running logos-module-client tests..."
            ${testsPkg}/bin/module_client_tests --gtest_output=xml:$out/test-results.xml
          '';
        }
      );

      devShells = forAllSystems ({ pkgs, ... }: {
        default = pkgs.mkShell {
          nativeBuildInputs = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
          ];
          buildInputs = [
            pkgs.qt6.qtbase
            pkgs.qt6.qtremoteobjects
            pkgs.gtest
            # Pulled in transitively by logos-cpp-sdk's plain-C++
            # TCP+SSL transport. Listing them here keeps `nix develop`
            # in sync with the build (find_package(OpenSSL/Boost) at
            # configure time would otherwise fail outside `nix build`).
            pkgs.openssl
            pkgs.boost
            pkgs.nlohmann_json
          ];
        };
      });
    };
}

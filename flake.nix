{
  description = "Logos Module Client - Proxy API and SDK client library";

  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    logos-cpp-sdk.inputs.logos-protocol.follows = "logos-protocol";
    # Master-tracking. This is the single protocol pin for the whole closure:
    # logos-plugin-qt and logos-cpp-sdk both `follow` it, so exactly one
    # logos-protocol ends up in the build. The per-client token store
    # (TokenManager::forIdentity / isolateIdentity) that logos-qt-host calls
    # is on master as of logos-protocol#59.
    logos-protocol = {
      url = "github:logos-co/logos-protocol";
      inputs.logos-nix.follows = "logos-nix";
    };
    # The Qt HOST RUNTIME (LogosAPI) this client layers on, published as
    # `packages.<sys>.logos-qt-host` with the CMake target
    # `logos-qt-host::logos_qt_host`. It used to ship from logos-qt-sdk as
    # `logos-qt-sdk::logos_qt_sdk`; it was the only thing this repo ever took
    # from there (no consumer emitter, no LpBridge headers, no UI plugin
    # context), so that input is gone rather than kept alongside.
    # `LogosAPIClient` / `LogosObject` are logos-protocol types, reached via
    # LOGOS_PROTOCOL_ROOT. Master-tracking as of logos-plugin-qt#19.
    logos-plugin-qt = {
      url = "github:logos-co/logos-plugin-qt";
      inputs.logos-nix.follows = "logos-nix";
      inputs.logos-protocol.follows = "logos-protocol";
    };
  };

  outputs = { self, nixpkgs, logos-nix, logos-cpp-sdk, logos-protocol, logos-plugin-qt }:
    let
      systems = [ "aarch64-darwin" "x86_64-darwin" "aarch64-linux" "x86_64-linux" ];
      forAllSystems = f: nixpkgs.lib.genAttrs systems (system: f {
        inherit system;
        pkgs = import nixpkgs { inherit system; };
        logosSdk = logos-cpp-sdk.packages.${system}.default;
        logosProtocolPkg = logos-protocol.packages.${system}.default;
        logosQtHost = logos-plugin-qt.packages.${system}.logos-qt-host;
      });
    in
    {
      packages = forAllSystems ({ pkgs, system, logosSdk, logosProtocolPkg, logosQtHost }:
        let
          common = import ./nix/default.nix { inherit pkgs logosSdk logosProtocolPkg logosQtHost; };
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

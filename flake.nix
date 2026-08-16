{
  description = "Logos Module Client - Proxy API and SDK client library";

  inputs = {
    logos-nix.url = "github:logos-co/logos-nix";
    nixpkgs.follows = "logos-nix/nixpkgs";
    logos-cpp-sdk.url = "github:logos-co/logos-cpp-sdk";
    logos-cpp-sdk.inputs.logos-protocol.follows = "logos-protocol";
    # Rev-pinned, not master-tracking: logos-qt-host (below) calls
    # TokenManager::forIdentity / isolateIdentity, which live on
    # logos-protocol's feat/per-client-token-store branch and are NOT on its
    # master. Both logos-plugin-qt and logos-cpp-sdk `follow` THIS input, so a
    # master-tracking pin here would build the Qt host runtime against a
    # protocol that lacks those symbols. c8bab12 is a fast-forward from master,
    # so nothing on master is given up. Drop the rev once it merges.
    logos-protocol = {
      url = "github:logos-co/logos-protocol/c8bab12834dbf92155b483546875e6078d17c74e";
      inputs.logos-nix.follows = "logos-nix";
    };
    # The Qt HOST RUNTIME (LogosAPI / LogosAPIClient / LogosObject) this
    # client layers on. It used to come from logos-qt-sdk; the runtime moved
    # to logos-plugin-qt, published as `packages.<sys>.logos-qt-host` with the
    # CMake target `logos-qt-host::logos_qt_host`. That is the only thing this
    # repo ever took from logos-qt-sdk (no consumer emitter, no LpBridge
    # headers), so the qt-sdk input is gone rather than kept alongside.
    #
    # Rev-pinned for the same reason logos-protocol is: `logos-qt-host` does
    # not exist on logos-plugin-qt's master (8846fc5) — a master-tracking url
    # fails to evaluate with "attribute 'logos-qt-host' missing". cc24fa1 is
    # the tip of that repo's feat/b4-qt-host-windows-target, already rebased
    # onto its master. It is the SUPERSET of the two branches carrying this
    # work; the sibling feat/b4-qt-host-windows-target-8ccb1fc (989f6ae) omits
    # commits that logos-module-builder pins, so pinning the superset here is
    # what keeps one logos-qt-host in the downstream closure instead of two.
    # Drop the rev once it merges.
    logos-plugin-qt = {
      url = "github:logos-co/logos-plugin-qt/cc24fa1c0c43b2d96c1dc165ee545a0321318b59";
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

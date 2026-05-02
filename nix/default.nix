# Common build configuration shared across all packages
{ pkgs, logosSdk }:

{
  pname = "logos-module-client";
  version = "0.1.0";

  # Common native build inputs
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.pkg-config
    pkgs.qt6.wrapQtAppsNoGuiHook
  ];

  # Common runtime dependencies. Qt6, Boost, OpenSSL, and nlohmann_json
  # come in transitively via logosSdk's `propagatedBuildInputs`
  # (declared on the SDK's symlinkJoin in logos-cpp-sdk/flake.nix);
  # listing them here would be redundant. The SDK's CMake Config
  # re-runs find_dependency(...) against those propagated entries at
  # configure time and stitches them into the imported target.
  buildInputs = [
    logosSdk
    pkgs.gtest
  ];

  # Common CMake flags
  cmakeFlags = [
    "-GNinja"
    "-DLOGOS_CPP_SDK_ROOT=${logosSdk}"
  ];

  # Environment variables
  env = {
    LOGOS_CPP_SDK_ROOT = "${logosSdk}";
  };

  # Metadata
  meta = with pkgs.lib; {
    description = "Logos Module Client - Proxy API and SDK client library";
    platforms = platforms.unix;
  };
}

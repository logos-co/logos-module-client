# Installs the logos-module-client headers
{ pkgs, common, src }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-headers";
  version = common.version;

  inherit src;
  inherit (common) meta;

  dontBuild = true;
  dontConfigure = true;

  installPhase = ''
    runHook preInstall

    mkdir -p $out/include

    for file in logos_module_client.h proxy_api.h \
                logos_core_client.h logos_core_client.cpp \
                logos_json_utils.h logos_json_utils.cpp \
                logos_sdk_c.h logos_sdk_c.cpp; do
      if [ -f src/$file ]; then
        cp src/$file $out/include/
      fi
    done

    runHook postInstall
  '';
}

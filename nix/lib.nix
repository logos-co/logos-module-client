# Extracts libraries from the shared build
{ pkgs, common, build }:

pkgs.runCommand "${common.pname}-lib-${common.version}"
  {
    inherit (common) meta;
  }
  ''
    mkdir -p $out/lib
    if [ -d ${build}/lib ]; then
      cp -r ${build}/lib/* $out/lib/
    fi
  ''

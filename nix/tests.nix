# Builds tests
{ pkgs, common, build }:

pkgs.stdenv.mkDerivation {
  pname = "${common.pname}-tests";
  version = common.version;

  inherit (build) src;
  inherit (common) buildInputs meta;

  nativeBuildInputs = common.nativeBuildInputs
    ++ pkgs.lib.optionals pkgs.stdenv.isDarwin [ pkgs.darwin.cctools ]
    ++ pkgs.lib.optionals pkgs.stdenv.isLinux [ pkgs.patchelf ];

  cmakeFlags = common.cmakeFlags;

  configurePhase = ''
    runHook preConfigure

    cp -r ${build}/* .
    chmod -R u+w .

    cmake -B build -S ${build.src} \
      -GNinja \
      -DLOGOS_CPP_SDK_ROOT=${common.env.LOGOS_CPP_SDK_ROOT} \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$out

    runHook postConfigure
  '';

  buildPhase = ''
    runHook preBuild

    cd build
    ninja module_client_tests

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    mkdir -p $out/bin
    cp bin/module_client_tests $out/bin/

    mkdir -p $out/lib
    cp -r lib/* $out/lib/ || true

    ${pkgs.lib.optionalString pkgs.stdenv.isLinux ''
      # rpath must cover every runtime lib the binary transitively pulls in.
      # Qt + gtest + libstdc++ is what this test binary used to need; since
      # the SDK grew a plain-C++ TLS transport we also link OpenSSL (libssl,
      # libcrypto) and Boost transitively, so both must be on rpath or the
      # wrapped binary dies with `libssl.so.3: cannot open shared object`.
      patchelf --set-rpath "$out/lib:${pkgs.gtest}/lib:${pkgs.qt6.qtbase}/lib:${pkgs.qt6.qtremoteobjects}/lib:${pkgs.openssl.out}/lib:${pkgs.boost}/lib:${pkgs.stdenv.cc.cc.lib}/lib" $out/bin/module_client_tests || true
    ''}

    runHook postInstall
  '';
}

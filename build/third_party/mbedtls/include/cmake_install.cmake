# Install script for directory: /tmp/opencode/tfire/third_party/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/aes.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/aria.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/asn1.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/asn1write.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/base64.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/bignum.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/block_cipher.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/build_info.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/camellia.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ccm.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/chacha20.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/chachapoly.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/check_config.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/cipher.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/cmac.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/compat-2.x.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_legacy_crypto.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_legacy_from_psa.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_psa_from_legacy.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_psa_superset_legacy.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_ssl.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_adjust_x509.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/config_psa.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/constant_time.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ctr_drbg.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/debug.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/des.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/dhm.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ecdh.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ecdsa.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ecjpake.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ecp.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/entropy.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/error.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/gcm.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/hkdf.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/hmac_drbg.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/lms.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/mbedtls_config.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/md.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/md5.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/memory_buffer_alloc.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/net_sockets.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/nist_kw.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/oid.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/pem.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/pk.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/pkcs12.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/pkcs5.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/pkcs7.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/platform.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/platform_time.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/platform_util.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/poly1305.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/private_access.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/psa_util.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ripemd160.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/rsa.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/sha1.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/sha256.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/sha3.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/sha512.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ssl.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ssl_cache.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ssl_cookie.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/ssl_ticket.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/threading.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/timing.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/version.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/x509.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/x509_crl.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/x509_crt.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/psa" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/build_info.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_adjust_auto_enabled.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_adjust_config_dependencies.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_adjust_config_key_pair_types.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_adjust_config_synonyms.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_builtin_composites.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_builtin_key_derivation.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_builtin_primitives.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_compat.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_config.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_driver_common.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_driver_contexts_composites.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_driver_contexts_key_derivation.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_driver_contexts_primitives.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_extra.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_legacy.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_platform.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_se_driver.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_sizes.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_struct.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_types.h"
    "/tmp/opencode/tfire/third_party/mbedtls/include/psa/crypto_values.h"
    )
endif()


const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const openssl = b.dependency("openssl", .{
        .target = target,
        .optimize = optimize,
    });

    const curl_c = b.dependency("curl", .{
        .target = target,
        .optimize = optimize,
    });

    const curl = b.addStaticLibrary(.{
        .name = "curl",
        .target = target,
        .optimize = optimize,
    });

    curl.linkLibC();
    curl.linkLibrary(openssl.artifact("ssl"));
    // curl.linkLibrary(openssl.artifact("crypto"));

    curl.root_module.addCMacro("BUILDING_LIBCURL", "");
    curl.root_module.addCMacro("CURL_STATICLIB", "1");
    curl.root_module.addCMacro("USE_OPENSSL", "1");
    curl.root_module.addCMacro("CURL_DISABLE_ALTSVC", "1");
    curl.root_module.addCMacro("CURL_DISABLE_COOKIES", "1");
    curl.root_module.addCMacro("CURL_DISABLE_BASIC_AUTH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_BEARER_AUTH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_DIGEST_AUTH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_KERBEROS_AUTH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_NEGOTIATE_AUTH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_AWS", "1");
    curl.root_module.addCMacro("CURL_DISABLE_DICT", "1");
    curl.root_module.addCMacro("CURL_DISABLE_DOH", "1");
    curl.root_module.addCMacro("CURL_DISABLE_FILE", "1");
    curl.root_module.addCMacro("CURL_DISABLE_FORM_API", "1");
    curl.root_module.addCMacro("CURL_DISABLE_FTP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_GETOPTIONS", "1");
    curl.root_module.addCMacro("CURL_DISABLE_GOPHER", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_HEADERS_API", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_HSTS", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_HTTP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_IMAP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_LDAP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_LDAPS", "1");
    curl.root_module.addCMacro("CURL_DISABLE_LIBCURL_OPTION", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_MIME", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_BINDLOCAL", "1");
    curl.root_module.addCMacro("CURL_DISABLE_MQTT", "1");
    curl.root_module.addCMacro("CURL_DISABLE_NETRC", "1");
    curl.root_module.addCMacro("CURL_DISABLE_NTLM", "1");
    curl.root_module.addCMacro("CURL_DISABLE_PARSEDATE", "1");
    curl.root_module.addCMacro("CURL_DISABLE_POP3", "1");
    curl.root_module.addCMacro("CURL_DISABLE_PROGRESS_METER", "1");
    curl.root_module.addCMacro("CURL_DISABLE_PROXY", "1");
    curl.root_module.addCMacro("CURL_DISABLE_RTSP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_SMB", "1");
    curl.root_module.addCMacro("CURL_DISABLE_SMTP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_SOCKETPAIR", "1");
    curl.root_module.addCMacro("CURL_DISABLE_TELNET", "1");
    curl.root_module.addCMacro("CURL_DISABLE_TFTP", "1");
    curl.root_module.addCMacro("CURL_DISABLE_VERBOSE_STRINGS", "1");
    // curl.root_module.addCMacro("CURL_DISABLE_OPENSSL_AUTO_LOAD_CONFIG", "1");

    if (target.result.os.tag == .windows) {
        curl.linkSystemLibrary("bcrypt");
    } else {
        curl.root_module.addCMacro(
            "CURL_EXTERN_SYMBOL",
            "__attribute__ ((__visibility__ (\"default\"))"
        );

        const isDarwin = target.result.isDarwin();
        if (!isDarwin)
            curl.root_module.addCMacro("ENABLE_IPV6", "1");
        curl.root_module.addCMacro("HAVE_ALARM", "1");
        curl.root_module.addCMacro("HAVE_ALLOCA_H", "1");
        curl.root_module.addCMacro("HAVE_ARPA_INET_H", "1");
        curl.root_module.addCMacro("HAVE_ARPA_TFTP_H", "1");
        curl.root_module.addCMacro("HAVE_ASSERT_H", "1");
        curl.root_module.addCMacro("HAVE_BASENAME", "1");
        curl.root_module.addCMacro("HAVE_BOOL_T", "1");
        curl.root_module.addCMacro("HAVE_BUILTIN_AVAILABLE", "1");
        curl.root_module.addCMacro("HAVE_CLOCK_GETTIME_MONOTONIC", "1");
        curl.root_module.addCMacro("HAVE_DLFCN_H", "1");
        curl.root_module.addCMacro("HAVE_ERRNO_H", "1");
        curl.root_module.addCMacro("HAVE_FCNTL", "1");
        curl.root_module.addCMacro("HAVE_FCNTL_H", "1");
        curl.root_module.addCMacro("HAVE_FCNTL_O_NONBLOCK", "1");
        curl.root_module.addCMacro("HAVE_FREEADDRINFO", "1");
        curl.root_module.addCMacro("HAVE_FTRUNCATE", "1");
        curl.root_module.addCMacro("HAVE_GETADDRINFO", "1");
        curl.root_module.addCMacro("HAVE_GETEUID", "1");
        curl.root_module.addCMacro("HAVE_GETPPID", "1");
        curl.root_module.addCMacro("HAVE_GETHOSTBYNAME", "1");
        if (!isDarwin)
            curl.root_module.addCMacro("HAVE_GETHOSTBYNAME_R", "1");
        curl.root_module.addCMacro("HAVE_GETHOSTBYNAME_R_6", "1");
        curl.root_module.addCMacro("HAVE_GETHOSTNAME", "1");
        curl.root_module.addCMacro("HAVE_GETPPID", "1");
        curl.root_module.addCMacro("HAVE_GETPROTOBYNAME", "1");
        curl.root_module.addCMacro("HAVE_GETPEERNAME", "1");
        curl.root_module.addCMacro("HAVE_GETSOCKNAME", "1");
        curl.root_module.addCMacro("HAVE_IF_NAMETOINDEX", "1");
        curl.root_module.addCMacro("HAVE_GETPWUID", "1");
        curl.root_module.addCMacro("HAVE_GETPWUID_R", "1");
        curl.root_module.addCMacro("HAVE_GETRLIMIT", "1");
        curl.root_module.addCMacro("HAVE_GETTIMEOFDAY", "1");
        curl.root_module.addCMacro("HAVE_GMTIME_R", "1");
        curl.root_module.addCMacro("HAVE_IFADDRS_H", "1");
        curl.root_module.addCMacro("HAVE_INET_ADDR", "1");
        curl.root_module.addCMacro("HAVE_INET_PTON", "1");
        curl.root_module.addCMacro("HAVE_SA_FAMILY_T", "1");
        curl.root_module.addCMacro("HAVE_INTTYPES_H", "1");
        curl.root_module.addCMacro("HAVE_IOCTL", "1");
        curl.root_module.addCMacro("HAVE_IOCTL_FIONBIO", "1");
        curl.root_module.addCMacro("HAVE_IOCTL_SIOCGIFADDR", "1");
        curl.root_module.addCMacro("HAVE_LDAP_URL_PARSE", "1");
        curl.root_module.addCMacro("HAVE_LIBGEN_H", "1");
        curl.root_module.addCMacro("HAVE_IDN2_H", "1");
        curl.root_module.addCMacro("HAVE_LL", "1");
        curl.root_module.addCMacro("HAVE_LOCALE_H", "1");
        curl.root_module.addCMacro("HAVE_LOCALTIME_R", "1");
        curl.root_module.addCMacro("HAVE_LONGLONG", "1");
        curl.root_module.addCMacro("HAVE_MALLOC_H", "1");
        curl.root_module.addCMacro("HAVE_MEMORY_H", "1");
        if (!isDarwin)
            curl.root_module.addCMacro("HAVE_MSG_NOSIGNAL", "1");
        curl.root_module.addCMacro("HAVE_NETDB_H", "1");
        curl.root_module.addCMacro("HAVE_NETINET_IN_H", "1");
        curl.root_module.addCMacro("HAVE_NETINET_TCP_H", "1");

        if (target.result.os.tag == .linux)
            curl.root_module.addCMacro("HAVE_LINUX_TCP_H", "1");
        curl.root_module.addCMacro("HAVE_NET_IF_H", "1");
        curl.root_module.addCMacro("HAVE_PIPE", "1");
        curl.root_module.addCMacro("HAVE_POLL", "1");
        curl.root_module.addCMacro("HAVE_POLL_FINE", "1");
        curl.root_module.addCMacro("HAVE_POLL_H", "1");
        curl.root_module.addCMacro("HAVE_POSIX_STRERROR_R", "1");
        curl.root_module.addCMacro("HAVE_PTHREAD_H", "1");
        curl.root_module.addCMacro("HAVE_PWD_H", "1");
        curl.root_module.addCMacro("HAVE_RECV", "1");
        curl.root_module.addCMacro("HAVE_SELECT", "1");
        curl.root_module.addCMacro("HAVE_SEND", "1");
        curl.root_module.addCMacro("HAVE_FSETXATTR", "1");
        curl.root_module.addCMacro("HAVE_FSETXATTR_5", "1");
        curl.root_module.addCMacro("HAVE_SETJMP_H", "1");
        curl.root_module.addCMacro("HAVE_SETLOCALE", "1");
        curl.root_module.addCMacro("HAVE_SETRLIMIT", "1");
        curl.root_module.addCMacro("HAVE_SETSOCKOPT", "1");
        curl.root_module.addCMacro("HAVE_SIGACTION", "1");
        curl.root_module.addCMacro("HAVE_SIGINTERRUPT", "1");
        curl.root_module.addCMacro("HAVE_SIGNAL", "1");
        curl.root_module.addCMacro("HAVE_SIGNAL_H", "1");
        curl.root_module.addCMacro("HAVE_SIGSETJMP", "1");
        curl.root_module.addCMacro("HAVE_SOCKADDR_IN6_SIN6_SCOPE_ID", "1");
        curl.root_module.addCMacro("HAVE_SOCKET", "1");
        curl.root_module.addCMacro("HAVE_STDBOOL_H", "1");
        curl.root_module.addCMacro("HAVE_STDINT_H", "1");
        curl.root_module.addCMacro("HAVE_STDIO_H", "1");
        curl.root_module.addCMacro("HAVE_STDLIB_H", "1");
        curl.root_module.addCMacro("HAVE_STRCASECMP", "1");
        curl.root_module.addCMacro("HAVE_STRDUP", "1");
        curl.root_module.addCMacro("HAVE_STRERROR_R", "1");
        curl.root_module.addCMacro("HAVE_STRINGS_H", "1");
        curl.root_module.addCMacro("HAVE_STRING_H", "1");
        curl.root_module.addCMacro("HAVE_STRSTR", "1");
        curl.root_module.addCMacro("HAVE_STRTOK_R", "1");
        curl.root_module.addCMacro("HAVE_STRTOLL", "1");
        curl.root_module.addCMacro("HAVE_STRUCT_SOCKADDR_STORAGE", "1");
        curl.root_module.addCMacro("HAVE_STRUCT_TIMEVAL", "1");
        curl.root_module.addCMacro("HAVE_SYS_IOCTL_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_PARAM_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_POLL_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_RESOURCE_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_SELECT_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_SOCKET_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_STAT_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_TIME_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_TYPES_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_UIO_H", "1");
        curl.root_module.addCMacro("HAVE_SYS_UN_H", "1");
        curl.root_module.addCMacro("HAVE_TERMIOS_H", "1");
        curl.root_module.addCMacro("HAVE_TERMIO_H", "1");
        curl.root_module.addCMacro("HAVE_TIME_H", "1");
        curl.root_module.addCMacro("HAVE_UNAME", "1");
        curl.root_module.addCMacro("HAVE_UNISTD_H", "1");
        curl.root_module.addCMacro("HAVE_UTIME", "1");
        curl.root_module.addCMacro("HAVE_UTIMES", "1");
        curl.root_module.addCMacro("HAVE_UTIME_H", "1");
        curl.root_module.addCMacro("HAVE_VARIADIC_MACROS_C99", "1");
        curl.root_module.addCMacro("HAVE_VARIADIC_MACROS_GCC", "1");
        curl.root_module.addCMacro("OS", "\"Linux\"");
        curl.root_module.addCMacro("RANDOM_FILE", "\"/dev/urandom\"");
        curl.root_module.addCMacro("RECV_TYPE_ARG1", "int");
        curl.root_module.addCMacro("RECV_TYPE_ARG2", "void *");
        curl.root_module.addCMacro("RECV_TYPE_ARG3", "size_t");
        curl.root_module.addCMacro("RECV_TYPE_ARG4", "int");
        curl.root_module.addCMacro("RECV_TYPE_RETV", "ssize_t");
        curl.root_module.addCMacro("SEND_QUAL_ARG2", "const");
        curl.root_module.addCMacro("SEND_TYPE_ARG1", "int");
        curl.root_module.addCMacro("SEND_TYPE_ARG2", "void *");
        curl.root_module.addCMacro("SEND_TYPE_ARG3", "size_t");
        curl.root_module.addCMacro("SEND_TYPE_ARG4", "int");
        curl.root_module.addCMacro("SEND_TYPE_RETV", "ssize_t");
        curl.root_module.addCMacro("SIZEOF_INT", "4");
        curl.root_module.addCMacro("SIZEOF_SHORT", "2");
        curl.root_module.addCMacro("SIZEOF_LONG", "8");
        curl.root_module.addCMacro("SIZEOF_OFF_T", "8");
        curl.root_module.addCMacro("SIZEOF_CURL_OFF_T", "8");
        curl.root_module.addCMacro("SIZEOF_SIZE_T", "8");
        curl.root_module.addCMacro("SIZEOF_TIME_T", "8");
        curl.root_module.addCMacro("STDC_HEADERS", "1");
        curl.root_module.addCMacro("TIME_WITH_SYS_TIME", "1");
        curl.root_module.addCMacro("USE_THREADS_POSIX", "1");
        curl.root_module.addCMacro("USE_UNIX_SOCKETS", "");
        curl.root_module.addCMacro("_FILE_OFFSET_BITS", "64");
    }

    curl.addIncludePath(curl_c.path("lib"));
    curl.addIncludePath(curl_c.path("include"));

    curl.addCSourceFiles(.{
        .root = curl_c.path("lib"),
        .files = &.{
            "vauth/krb5_sspi.c",
            "vauth/spnego_sspi.c",
            "vauth/ntlm.c",
            "vauth/gsasl.c",
            "vauth/spnego_gssapi.c",
            "vauth/ntlm_sspi.c",
            "vauth/vauth.c",
            "vauth/oauth2.c",
            "vauth/cram.c",
            "vauth/cleartext.c",
            "vauth/krb5_gssapi.c",
            "vauth/digest.c",
            "vauth/digest_sspi.c",
            "vquic/vquic.c",
            "vquic/curl_ngtcp2.c",
            // "vquic/curl_osslq.c",
            // "vquic/vquic-tls.c",
            "vquic/curl_quiche.c",
            "vquic/curl_msh3.c",
            "vssh/libssh.c",
            "vssh/libssh2.c",
            "vssh/wolfssh.c",
            // "vtls/mbedtls.c",
            "vtls/gtls.c",
            "vtls/bearssl.c",
            "vtls/hostcheck.c",
            "vtls/rustls.c",
            "vtls/schannel.c",
            "vtls/sectransp.c",
            "vtls/schannel_verify.c",
            "vtls/vtls.c",
            "vtls/cipher_suite.c",
            "vtls/keylog.c",
            "vtls/openssl.c",
            "vtls/wolfssl.c",
            "vtls/mbedtls_threadlock.c",
            "vtls/x509asn1.c",
            "strcase.c",
            "easyoptions.c",
            "dict.c",
            "llist.c",
            "mprintf.c",
            "pingpong.c",
            "socks_gssapi.c",
            "psl.c",
            "url.c",
            "timeval.c",
            "curl_get_line.c",
            "hmac.c",
            "md4.c",
            "curl_range.c",
            "idn.c",
            "hostsyn.c",
            "strtok.c",
            "curl_threads.c",
            "if2ip.c",
            "c-hyper.c",
            "cf-socket.c",
            "http_negotiate.c",
            "doh.c",
            "curl_endian.c",
            "formdata.c",
            "easygetopt.c",
            "cf-https-connect.c",
            "timediff.c",
            "dynbuf.c",
            "rand.c",
            "http2.c",
            "request.c",
            "dynhds.c",
            "content_encoding.c",
            "hostip.c",
            "escape.c",
            "version_win32.c",
            "easy.c",
            "rename.c",
            "share.c",
            "slist.c",
            "inet_pton.c",
            "tftp.c",
            "mqtt.c",
            "fopen.c",
            "socks.c",
            "parsedate.c",
            "curl_trc.c",
            "warnless.c",
            "cf-haproxy.c",
            "cfilters.c",
            "curl_sha512_256.c",
            "system_win32.c",
            "transfer.c",
            "curl_rtmp.c",
            "nonblock.c",
            "select.c",
            "hostip4.c",
            "http1.c",
            "urlapi.c",
            "openldap.c",
            "getenv.c",
            "hash.c",
            "bufq.c",
            "http_proxy.c",
            "krb5.c",
            "multi.c",
            "strdup.c",
            "mime.c",
            "socks_sspi.c",
            "smtp.c",
            "http_aws_sigv4.c",
            "curl_addrinfo.c",
            "cf-h2-proxy.c",
            "memdebug.c",
            "progress.c",
            "curl_ntlm_core.c",
            "curl_path.c",
            "hostasyn.c",
            "pop3.c",
            "noproxy.c",
            "gopher.c",
            "rtsp.c",
            "curl_gethostname.c",
            "curl_des.c",
            "base64.c",
            "splay.c",
            "http.c",
            "curl_sspi.c",
            "http_chunks.c",
            "telnet.c",
            "amigaos.c",
            "fileinfo.c",
            "version.c",
            "ldap.c",
            "bufref.c",
            "curl_sasl.c",
            "netrc.c",
            "socketpair.c",
            "strerror.c",
            "curl_multibyte.c",
            "altsvc.c",
            "conncache.c",
            "curl_memrchr.c",
            "dllmain.c",
            "smb.c",
            "sha256.c",
            "connect.c",
            "ws.c",
            "ftp.c",
            "strtoofft.c",
            "md5.c",
            "file.c",
            "http_digest.c",
            "asyn-thread.c",
            "cf-h1-proxy.c",
            "hsts.c",
            "asyn-ares.c",
            "imap.c",
            "headers.c",
            "macos.c",
            "cookie.c",
            "hostip6.c",
            "sendf.c",
            "ftplistparser.c",
            "http_ntlm.c",
            "setopt.c",
            "getinfo.c",
            "speedcheck.c",
            "inet_ntop.c",
            "cw-out.c",
            "curl_gssapi.c",
            "curl_fnmatch.c",
        },
        .flags = &.{
            "-std=gnu89",
            "-Wno-unknown-warning-option",
            "-Wswitch-default",
            "-Wno-parentheses-equality",
            "-Wno-language-extension-token",
            "-Wno-extended-offsetof",
            "-Wconditional-uninitialized",
            "-Wincompatible-pointer-types-discards-qualifiers",
            "-Wmissing-variable-declarations",
            "-Wno-int-conversion",
        },
    });

    curl.installHeadersDirectory(curl_c.path("include/curl"), "curl", .{});

    b.installArtifact(curl);
}

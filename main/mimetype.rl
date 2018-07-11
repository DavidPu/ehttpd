#include <stddef.h>
%%{
    machine mime_type_lookup;
    main := |*

  ######################################################################################
  # from https://raw.githubusercontent.com/h5bp/server-configs-nginx/master/mime.types #
  ######################################################################################

  # Data interchange


    "atom" { return "application/atom+xml"; };
    "json"|"map"|"topojson" { return "application/json"; };
    "jsonld" { return "application/ld+json"; };
    "rss" { return "application/rss+xml"; };
    "geojson" { return "application/vnd.geo+json"; };
    "rdf"|"xml" { return "application/xml"; };


  # JavaScript


    # Normalize to standard type.

    # https://tools.ietf.org/html/rfc4329#section-7.2

    "js" { return "application/javascript"; };


  # Manifest files


    "webmanifest" { return "application/manifest+json"; };
    "webapp" { return "application/x-web-app-manifest+json"; };
    "appcache" { return "text/cache-manifest"; };


  # Media files


    "mid"|"midi"|"kar" { return "audio/midi"; };
    "aac"|"f4a"|"f4b"|"m4a" { return "audio/mp4"; };
    "mp3" { return "audio/mpeg"; };
    "oga"|"ogg"|"opus" { return "audio/ogg"; };
    "ra" { return "audio/x-realaudio"; };
    "wav" { return "audio/x-wav"; };
    "bmp" { return "image/bmp"; };
    "gif" { return "image/gif"; };
    "jpeg"|"jpg" { return "image/jpeg"; };
    "jxr"|"hdp"|"wdp" { return "image/jxr"; };
    "png" { return "image/png"; };
    "svg"|"svgz" { return "image/svg+xml"; };
    "tif"|"tiff" { return "image/tiff"; };
    "wbmp" { return "image/vnd.wap.wbmp"; };
    "webp" { return "image/webp"; };
    "jng" { return "image/x-jng"; };
    "3gp"|"3gpp" { return "video/3gpp"; };
    "f4p"|"f4v"|"m4v"|"mp4" { return "video/mp4"; };
    "mpeg"|"mpg" { return "video/mpeg"; };
    "ogv" { return "video/ogg"; };
    "mov" { return "video/quicktime"; };
    "webm" { return "video/webm"; };
    "flv" { return "video/x-flv"; };
    "mng" { return "video/x-mng"; };
    "asf"|"asx" { return "video/x-ms-asf"; };
    "wmv" { return "video/x-ms-wmv"; };
    "avi" { return "video/x-msvideo"; };

    # Serving `.ico` image files with a different media type

    # prevents Internet Explorer from displaying then as images:

    # https://github.com/h5bp/html5-boilerplate/commit/37b5fec090d00f38de64b591bcddcb205aadf8ee


    "cur"|"ico" { return "image/x-icon"; };


  # Microsoft Office


    "doc" { return "application/msword"; };
    "xls" { return "application/vnd.ms-excel"; };
    "ppt" { return "application/vnd.ms-powerpoint"; };
    "docx" { return "application/vnd.openxmlformats-officedocument.wordprocessingml.document"; };
    "xlsx" { return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"; };
    "pptx" { return "application/vnd.openxmlformats-officedocument.presentationml.presentation"; };


  # Web fonts


    "woff" { return "application/font-woff"; };
    "woff2" { return "application/font-woff2"; };
    "eot" { return "application/vnd.ms-fontobject"; };

    # Browsers usually ignore the font media types and simply sniff

    # the bytes to figure out the font type.

    # https://mimesniff.spec.whatwg.org/#matching-a-font-type-pattern

    #

    # However, Blink and WebKit based browsers will show a warning

    # in the console if the following font types are served with any

    # other media types.


    "ttc"|"ttf" { return "application/x-font-ttf"; };
    "otf" { return "font/opentype"; };


  # Other


    "ear"|"jar"|"war" { return "application/java-archive"; };
    "hqx" { return "application/mac-binhex40"; };
    "bin"|"deb"|"dll"|"dmg"|"exe"|"img"|"iso"|"msi"|"msm"|"msp"|"safariextz" { return "application/octet-stream"; };
    "pdf" { return "application/pdf"; };
    "ai"|"eps"|"ps" { return "application/postscript"; };
    "rtf" { return "application/rtf"; };
    "kml" { return "application/vnd.google-earth.kml+xml"; };
    "kmz" { return "application/vnd.google-earth.kmz"; };
    "wmlc" { return "application/vnd.wap.wmlc"; };
    "7z" { return "application/x-7z-compressed"; };
    "bbaw" { return "application/x-bb-appworld"; };
    "torrent" { return "application/x-bittorrent"; };
    "crx" { return "application/x-chrome-extension"; };
    "cco" { return "application/x-cocoa"; };
    "jardiff" { return "application/x-java-archive-diff"; };
    "jnlp" { return "application/x-java-jnlp-file"; };
    "run" { return "application/x-makeself"; };
    "oex" { return "application/x-opera-extension"; };
    "pl"|"pm" { return "application/x-perl"; };
    "pdb"|"prc" { return "application/x-pilot"; };
    "rar" { return "application/x-rar-compressed"; };
    "rpm" { return "application/x-redhat-package-manager"; };
    "sea" { return "application/x-sea"; };
    "swf" { return "application/x-shockwave-flash"; };
    "sit" { return "application/x-stuffit"; };
    "tcl"|"tk" { return "application/x-tcl"; };
    "crt"|"der"|"pem" { return "application/x-x509-ca-cert"; };
    "xpi" { return "application/x-xpinstall"; };
    "xhtml" { return "application/xhtml+xml"; };
    "xsl" { return "application/xslt+xml"; };
    "zip" { return "application/zip"; };
    "css" { return "text/css"; };
    "csv" { return "text/csv"; };
    "htm"|"html"|"shtml" { return "text/html"; };
    "md" { return "text/markdown"; };
    "mml" { return "text/mathml"; };
    "txt" { return "text/plain"; };
    "vcard"|"vcf" { return "text/vcard"; };
    "xloc" { return "text/vnd.rim.location.xloc"; };
    "jad" { return "text/vnd.sun.j2me.app-descriptor"; };
    "wml" { return "text/vnd.wap.wml"; };
    "vtt" { return "text/vtt"; };
    "htc" { return "text/x-component"; };


    *|;
}%%

%% write data nofinal;
const char *get_mime_type(const char *suffix, size_t length)
{
    const char *p   = suffix;
    const char *pe  = p + length;
    const char *eof = pe;
    const char *ts;
    const char *te;
    int cs;
    int act;

    (void) mime_type_lookup_error;
    (void) mime_type_lookup_en_main;
    (void)ts;
    (void)act;

    %% write init;
    %% write exec;
    return NULL;
}

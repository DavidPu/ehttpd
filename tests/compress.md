find /var/www/static -type f -regextype posix-extended -iregex '.*\.(css|csv|html?|js|svg|txt|xml)' -exec zopfli '{}' \;

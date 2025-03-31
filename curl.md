[curl tutorial](https://curl.se/docs/tutorial.html)  
[curl man page](https://curl.se/docs/manpage.html)  
[Everything curl](https://ec.haxx.se/index.html)  

## Options
```bash
-I, --head
    Fetch the headers only

--connect-timeout <seconds>
    Maximum time in seconds that you allow curl's connection to take. This only limits the connection phase, so if curl connects within the given period it continues - if not it exits.

-x, --proxy [protocol://]host[:port]
    Use the specified proxy
```

## Examples
```bash
# 1
if curl -I --connect-timeout 3 --proxy 135.242.60.169:58080 https://www.google.com > /dev/null 2>&1; then
    echo "ping google successfully"
else
    echo "can not ping google"
fi
```

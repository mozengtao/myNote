#!/usr/bin/env bash
#
# Primitive 层 - 打包 / 远端解包原语。

# archive_app
# 把本地 APP_SRC 目录打包成 ARCHIVE。
# 真实：tar czf "$ARCHIVE" "$APP_SRC"/
archive_app()
{
    echo "    [tar] czf $ARCHIVE $APP_SRC/"
}

# deploy_archive HOST
# 在远端把上传好的归档解包到 APP_DIR。
# 复用 remote_exec，自己完全不碰 ssh 细节。
deploy_archive()
{
    local host=$1

    remote_exec "$host" "rm -rf $APP_DIR && mkdir -p $APP_DIR && tar xzf /tmp/$ARCHIVE -C $APP_DIR"
}

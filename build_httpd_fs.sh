#!/bin/sh
#
# build_httpd_fs.sh
#

SERVER="BrickPico (https://github.com/tjko/brickpico)"

FSDIR=src/httpd-fs/
FSDATAFILE=src/brickpico_fsdata.c

fatal() { echo "`basename $0`: $*"; exit 1; }

[ -d "$FSDIR" ] || fatal "cannot find fs directory: $FSDIR"


./contrib/makefsdata ${FSDIR} -m -svr:"${SERVER}" -ssi:src/httpd-fs_ssi.list -f:${FSDATAFILE} -x:html~,shtml~,json~,~
[ $? -eq 0 ] || fatal "makefsdata failed"

dos2unix ${FSDATAFILE}
[ $? -eq 0 ] || fatal "dos2unix failed"

# HACK: make sure next build will recompile fanpico_fsdata.c...
find build/ -type f -name 'fs.c.obj' -print -delete



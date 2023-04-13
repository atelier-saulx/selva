#!/bin/sh
#SELVA_MALLOC_CONF="prof:true,prof_active:true,prof_leak:true,lg_prof_interval:30,lg_prof_sample:17,prof_prefix:jeprof.out"
LOCPATH=../../../binaries/Linux_x86_64/locale SELVA_PORT=3000 SELVA_REPLICATION_MODE=1 AUTO_SAVE_INTERVAL=1200 exec ../../../selvad

#!/bin/bash
out=$(ps aux | grep redis)
for line in $out; do
  if [[ $line == '*:'* ]]; then
    port=${line:2}
    subs=$(redis-cli -p $port selva.subscriptions.list ___selva_hierarchy)
    if [[ $subs == '' ]]; then
      continue
    fi

    if [[ $subs == 'ERR'* ]]; then
      continue
    fi

    for sub in $subs; do
      echo "SUB $sub"
      redis-cli -p $port selva.subscriptions.debug ___selva_hierarchy $sub
    done
  fi
done

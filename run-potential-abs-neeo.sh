#!/usr/bin/env bash

./potential-abs -p 1 --save-to ./nppo_cluster_5000.card_abs --load-from ./nepo_cluster_5000.card_abs --threads 90 --nb-buckets 5000 --handranks=./handranks.dat

#!/usr/bin/env bash

./potential-abs -p 2 --save-to ./nepo_cluster_5000.card_abs --load-from ./neeo_cluster_10.card_abs --threads 90 --nb-buckets 5000 --handranks=./handranks.dat

#!/usr/bin/env bash

./potential-abs -p 2 --save-to ./nppo_cluster_full_5000.card_abs --load-from ./nppo_cluster_5000.card_abs9 --threads 90 --nb-buckets 5000 --handranks=./handranks.dat

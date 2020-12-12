# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(cache-hit) begin
(cache-hit) end
cache-hit: exit(0)
EOF
pass;
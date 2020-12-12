# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(coalesce) begin
(coalesce) success
(coalesce) end
cache-hit: exit(0)
EOF
pass;
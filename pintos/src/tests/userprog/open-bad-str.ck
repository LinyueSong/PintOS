# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(open-bad-str) begin
open-bad-str: exit(-1)
EOF
pass;
# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(tell-normal) begin
(tell-normal) end
tell-normal: exit(0)
EOF
pass;
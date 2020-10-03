# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(seek-normal) begin
(seek-normal) end
seek-normal: exit(0)
EOF
pass;
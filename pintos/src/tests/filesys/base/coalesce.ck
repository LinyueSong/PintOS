# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(coalesce) begin
(coalesce) success
(coalesce) end
coalesce: exit(0)
EOF
pass;
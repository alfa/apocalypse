
package Preprocessor;

our @ISA = qw(Exporter);
our @EXPORT_OK = qw(apply,appl_zone);
our $VERSION = 1.0;


use strict;
use Parsescp;

my $zoneid_map = {
    # zoneid => [y2, y1, x2, y1]
    "141" => [3814.58325195312,-1277.08325195312,11831.25,8437.5],
    "12" => [1535.41662597656,-1935.41662597656,-7939.5830078125,-10254.166015625],
    "406" => [3245.83325195312,-1637.49987792969,2916.66650390625,-339.583312988281],
    "15" => [-974.999938964844,-6225,-2033.33325195312,-5533.3330078125],
    "1377" => [4641.66650390625,-2308.33325195312,-5800,-10433.3330078125],
    "1519" => [1380.97143554688,36.7006301879883,-8278.8505859375,-9175.205078125],
    "1637" => [-3680.60107421875,-5083.20556640625,2273.87719726562,1338.46057128906],
    "1497" => [873.192626953125,-86.1824035644531,1877.9453125,1237.84118652344],
    "2597" => [1781.24987792969,-2456.25,1085.41662597656,-1739.58325195312],
    "357" => [5441.66650390625,-1508.33325195312,-2366.66650390625,-6999.99951171875],
    "440" => [-218.749984741211,-7118.74951171875,-5875,-10475],
    "14" => [-1962.49987792969,-7249.99951171875,1808.33325195312,-1716.66662597656],
    "215" => [2047.91662597656,-3089.58325195312,-272.916656494141,-3697.91650390625],
    "17" => [2622.91650390625,-7510.41650390625,1612.49987792969,-5143.75],
    "36" => [783.333312988281,-2016.66662597656,1500,-366.666656494141],
    "45" => [-866.666625976562,-4466.66650390625,-133.33332824707,-2533.33325195312],
    "3" => [-2079.16650390625,-4566.66650390625,-5889.5830078125,-7547.91650390625],
    "4" => [-1241.66662597656,-4591.66650390625,-10566.666015625,-12800],
    "85" => [3033.33325195312,-1485.41662597656,3837.49975585938,824.999938964844],
    "130" => [3449.99975585938,-750,1666.66662597656,-1133.33325195312],
    "28" => [416.666656494141,-3883.33325195312,3366.66650390625,499.999969482422],
    "139" => [-2185.41650390625,-6056.25,3799.99975585938,1218.75],
    "267" => [1066.66662597656,-2133.33325195312,400,-1733.33325195312],
    "47" => [-1575,-5425,1466.66662597656,-1100],
    "1" => [1802.08325195312,-3122.91650390625,-3877.08325195312,-7160.41650390625],
    "51" => [-322.916656494141,-2554.16650390625,-6100,-7587.49951171875],
    "46" => [-266.666656494141,-3195.83325195312,-7031.24951171875,-8983.3330078125],
    "41" => [-833.333312988281,-3333.33325195312,-9866.666015625,-11533.3330078125],
    "10" => [833.333312988281,-1866.66662597656,-9716.666015625,-11516.666015625],
    "38" => [-1993.74987792969,-4752.0830078125,-4487.5,-6327.0830078125],
    "44" => [-1570.83325195312,-3741.66650390625,-8575,-10022.916015625],
    "33" => [2220.83325195312,-4160.41650390625,-11168.75,-15422.916015625],
    "8" => [-2222.91650390625,-4516.66650390625,-9620.8330078125,-11150],
    "40" => [3016.66650390625,-483.333312988281,-9400,-11733.3330078125],
    "11" => [-389.583312988281,-4525,-2147.91650390625,-4904.16650390625],
    "148" => [2941.66650390625,-3608.33325195312,8333.3330078125,3966.66650390625],
    "331" => [1699.99987792969,-4066.66650390625,4672.91650390625,829.166625976562],
    "405" => [4233.3330078125,-262.5,452.083312988281,-2545.83325195312],
    "400" => [-433.333312988281,-4833.3330078125,-3966.66650390625,-6899.99951171875],
    "16" => [-3277.08325195312,-8347.916015625,5341.66650390625,1960.41662597656],
    "361" => [1641.66662597656,-4108.3330078125,7133.3330078125,3299.99975585938],
    "618" => [-316.666656494141,-7416.66650390625,8533.3330078125,3799.99975585938],
    "1537" => [-713.591369628906,-1504.21643066406,-4569.2412109375,-5096.845703125],
    "1657" => [2938.36279296875,1880.02954101562,10238.31640625,9532.5869140625],
    "3358" => [1858.33325195312,102.08332824707,1508.33325195312,337.5],
    "490" => [533.333312988281,-3166.66650390625,-5966.66650390625,-8433.3330078125],
    "3277" => [2041.66662597656,895.833312988281,1627.08325195312,862.499938964844],
    "493" => [-1381.25,-3689.58325195312,8491.666015625,6952.0830078125],
    "1638" => [516.666625976562,-527.083312988281,-849.999938964844,-1545.83325195312]
    };

sub get_spawn_points
{
    my ($obj_map, $spawn_points) = @_;
    if( ref($spawn_points) ne 'HASH' ) { die("Expect a hash reference for spawn points not $spawn_points\n"); }

    for my $key( keys %{$obj_map} )
    {
	my $spawn_obj = $obj_map->{$key}{"LINK"};
	if( $spawn_obj ne "" )
	{
	    # double check if its a spawn point... flunk ...
	    my $spawn_mark = $obj_map->{$spawn_obj}{"ENTRY"};
	    if( $spawn_mark eq "1" )
	    {
		$spawn_points->{$key} = $spawn_obj;
	    }
	}
    }
}

sub set_merge_template
{
    my ($obj_map_item, $merge_template, $merge_id) = @_;
    my $display_id = $obj_map_item->{"ENTRY"};
    if(  $display_id ne "" )
    {
	for my $key( keys %{$merge_template->{$display_id}}  )
	{
	    if( $key eq "attack" || $key  eq "damage" ) #HACK
	    {
		my @val_arr = split " ", $merge_template->{$display_id}{$key};
		my $size = scalar(@val_arr);
		for(my $i=0; $i < $size; ++$i)
		{
		    $obj_map_item->{$merge_id . $key . "_" . $i} = $val_arr[$i];		    
		}
	    }
	    else
	    {
		$obj_map_item->{$merge_id . "_" . $key} = $merge_template->{$display_id}{$key};
	    }
	}
    }
}

sub set_zone_id
{
    my ($obj_map_item) = @_;
    my @xyz = split ' ', $obj_map_item->{"Link_XYZ"};
   
    # x1 = [0], y1[1], x2[2], y2[3]
    my $size = scalar(@xyz);
    if( $size > 1 )
    {
	for my $zone_key( keys %{$zoneid_map})
	{
	    my $arr_ref = $zoneid_map->{$zone_key};
	    my $y2 = $arr_ref->[0];
	    my $y1 = $arr_ref->[1];
	    my $x2 = $arr_ref->[2];
	    my $x1 = $arr_ref->[3];

	    # if X1 <= posX <= X2 and Y1 <= posY <= Y2 then we have hit a home run.
	    if( $xyz[0] >= $x1 && $xyz[0] <= $x2 &&
		$xyz[1] >= $y1 && $xyz[1] <= $y2 )
	    {
		$obj_map_item->{"ZONE"} = $zone_key;
		return; # zone found.. done
	    }
	}

	my $drop_item = $obj_map_item->{"ENTRY"};
	printf "Dropping object: $drop_item\r\n";
    }
}

sub apply
{
    my ($obj_map, $lookup_map, $init_str) = @_;
    if( ref($obj_map) ne 'HASH' ) { die("Expect a hash reference for object map not $obj_map\n"); }

    my %spawn_points = {};
    get_spawn_points($obj_map, \%spawn_points);

    if( $init_str eq "true" || $init_str eq "yes" )
    {
	$init_str = "creatures.scp";
    }
    my $merge_id = "creature";

    if( $init_str =~ /^(.*).scp$/ )
    {
	$merge_id = $1;
    }

    my %merge_object;
    Parsescp::parse($init_str, \%merge_object, $lookup_map, "");

    for my $key ( keys %spawn_points )
    {
	if( $key ne "" )
	{
	    my $spawn_reference = $obj_map->{$spawn_points{$key}};
	    for my $ref_key( keys %{$spawn_reference} )
	    {
		$obj_map->{$key}{"Link_" . $ref_key} = $spawn_reference->{$ref_key};
	    }
	    set_zone_id($obj_map->{$key});
	    set_merge_template($obj_map->{$key}, \%merge_object, $merge_id);
	    $obj_map->{$spawn_points{$key}} = {}; # for performance purposes.	
	}
    }    
}

sub apply_zone
{
    my ($obj_map) = @_;

    for my $key( keys %{$obj_map} )
    {
	if( $key ne "" )
	{
	    my $obj_map_item = $obj_map->{$key};
	    my $entry_id = $obj_map_item->{"ENTRY"};
	    if( $entry_id eq 1 )
	    {
		# this is a spawn point
		$obj_map->{$key} = {}; 
	    }
	    else
	    {
		my @xyz = split ' ', $obj_map_item->{"XYZ"};
		
		# x1 = [0], y1[1], x2[2], y2[3]
		my $size = scalar(@xyz);
		if( $size > 1 )
		{
		    for my $zone_key( keys %{$zoneid_map})
		    {
			my $arr_ref = $zoneid_map->{$zone_key};
			my $y2 = $arr_ref->[0];
			my $y1 = $arr_ref->[1];
			my $x2 = $arr_ref->[2];
			my $x1 = $arr_ref->[3];
			
			# if X1 <= posX <= X2 and Y1 <= posY <= Y2 then we have hit a home run.
			if( $xyz[0] >= $x1 && $xyz[0] <= $x2 &&
			    $xyz[1] >= $y1 && $xyz[1] <= $y2 )
			{
			    $obj_map_item->{"ZONE"} = $zone_key;
			}
		    }	   
		}
	    }
	}
    }
}

<?php
// $Id: signup.inc,v 1.1.2.2.2.9 2008/01/24 13:04:37 dm Exp $
include_once("/home/dp/misc/db.inc");
include_once("/home/dp/misc/dp.inc");


// fields for test / replace / save
if (!isset($form_fields)) {
    $form_fields = array(
	"username" => "Username",
	"password" => "Password",
	"name1"    => "First name",
	"name2"    => "Last name",
	"addr"     => "Address",
	"city"     => "City",
	"state"    => "State",
	"zip"      => "Postal code",
	"country"  => "Country",
	"email"    => "Email"
    );
}


// tests site
if (!isset($sid) || count($db->query_row("SELECT site_id FROM sites WHERE type = 0 AND site_id = ".$sid)) == 0) {
    print "NOT SET or INVALID \$sid : '$sid'";
    exit;
}

// test billing support
$supp_bills = array(
    10, // CCBILL
    21, // Segpay
    24, // Epoch
    25, // 365Bill
    28  // GXBill
    );
$db->query("SELECT b_id, name FROM casc WHERE site_id = $sid");
while ($db->next_record()) {
    if (!in_array($db->r["b_id"], $supp_bills)) {
	print "ERROR : billing ".$db->r["name"]." (ID ".$db->r["b_id"].") not supported in cascade yet";
	exit;
    }
}

// init GeoIP
if (file_exists($geoip_dat)) {
    $gi = geoip_open($geoip_dat, GEOIP_STANDARD);
}
else {
    print "can't open GeoIP data file '$geoip_dat'\n";
    exit;
}


// domain for cookies
$domain = $_SERVER['HTTP_HOST'];
if (strpos($domain, "www.") != FALSE) {
    $domain = substr($domain, 4);
    $domain = ".".$domain;
}

function cookie($name, $value) {
    global $domain;
    setcookie($name, $value, time()+60*60*24, "/", $domain);
}


function check_fields() {
    global $POST, $domain, $form_fields, $vars, $errors;

    // empty fields
    foreach ($form_fields as $k => $v) {
	if (isset($_POST[$k])) {
	    if ($_POST[$k] == '') {
		$errors[$k] = $v." empty";
		if (!isset($errors['error'])) {
		    $errors['error'] = $v." empty";
		}
	    }
	    else {
		$vars[$k] = $_POST[$k];
	    }
	}
    }
}


function get_gg_id() {
    global $_GET, $_SERVER, $db, $gi;
    
    // default - Other group
    $gg_id = 1;
    
    // debug : override from request
    if (isset($_GET['country'])) {
	$cntr = $_GET['country'];
    }
    else {
	$cntr = geoip_country_code_by_addr($gi, $_SERVER["REMOTE_ADDR"]);
	$cntr = $cntr == "" ? "--" : $cntr;
    }

    $db->query("SELECT * FROM geo_groups WHERE gg_id > 1 ORDER BY gg_id DESC");
    while ($db->next_record()) {
	$cl = explode(",", $db->r["list"]);
	if (in_array($cntr, $cl)) {
	    $gg_id = $db->r["gg_id"];
	}
    }
    
    return $gg_id;
}


function get_c_id($ctype) {
    global $db, $sid;
    
    $gg_id = get_gg_id();

    // already used billings
    $c_excl = "";
    if (isset($_SESSION['cascade'][$ctype]) && $_SESSION['cascade'][$ctype] != "") {
    $c_excl = " AND c_id NOT IN (" . $_SESSION['cascade'][$ctype] . ")";
    }
    
    // get curls ids by order
    $bids = $cids = $redirs = array();
    $db->query("SELECT * FROM casc WHERE site_id = $sid AND type = $ctype ".$c_excl." ORDER BY pos ASC");
    while ($db->next_record()) {
	$c_id = $db->r["c_id"];
	$only = $db->r["gg_only"];
	$excl = $db->r["gg_excl"];
	// skip not matched geotarget groups
	if ($only != 0 && $only != $gg_id) continue;
	if ($excl != 0 && $excl == $gg_id) continue;
	// return billing
//	return array($c_id, $db->r["redir"]);
	array_push($bids, $db->r["b_id"]);
	array_push($cids, $db->r["c_id"]);
	array_push($redirs, $db->r["redir"]);
    }
    
    // no more billings
    if (count($bids) == 0) {
	return array(0, -1);
    }

    // custom bills order
    $id = $_SESSION['id'];
    $m_id = get_m_id($id);
    $db->query("SELECT * FROM cbo WHERE m_id = $m_id ORDER BY pos");
    while ($db->next_record()) {
	$bid = $db->r["b_id"];
	if (($n = array_search($bid, $bids)) !== FALSE) {
	    return array($cids[$n], $redirs[$n]);
	}
    }
    
    // first from global order
    return array($cids[0], $redirs[0]);
}


function get_def_ch_id() {
    global $db, $sid;

    $db->query_row("SELECT c_id FROM casc WHERE site_id = $sid AND type = 1 ORDER BY pos ASC");
    if ($db->nr() != 0) {
	return $db->r['c_id'];
    }
    return 0;
}

function get_def_ph_id() {
    global $db, $sid;

    $db->query_row("SELECT c_id FROM casc WHERE site_id = $sid AND type = 2 ORDER BY pos ASC");
    if ($db->nr() != 0) {
	return $db->r['c_id'];
    }
    return 0;
}

function curl_data($cu_id) {
    global $db, $form_fields, $vars;
    
    // urls
    $db->query_row("SELECT * FROM curls WHERE cu_id = $cu_id");
    $c_id = $db->r['c_id'];
    $name = $db->r['name'];
    $url = $db->r['url'];
    $pos = $db->r['pos'];
    // billing
    $db->query_row("SELECT * FROM casc WHERE c_id = $c_id");
    $b_id = $db->r['b_id'];
    $ctype = $db->r['type'];
    $bname = $db->r['name'];
    if ($url == '') {
	print "ERROR : empty URL for cascade option $cu_id ($bname, '$name')\n";

    }
    // remove linebreaks
    $url = str_replace("\r", "", $url);
    $url = str_replace("\n", "", $url);
    // subst form variables
    foreach ($form_fields as $k => $v) {
	$str = "::$k::";
	$url = str_replace($str, $vars[$k], $url);
    }
    // compose $sv_id and $ccb_id
    $crc = ref_crc($_SESSION['ref']);
    $sv_id = join("_", array($_SESSION['id'], $crc, $_SESSION['tr'], $_SESSION['ps']));
    // ccbill
    $id = $_SESSION['id'];
    $m_id = get_m_id($id);
    // back from alias
    $real_id = get_real_id($m_id);
    if (isset($real_id)) {
	$id = $real_id;
    }
    if (get_ccbill_id($id) != "") {
	$ccb_id = "ccbill_referer=".get_ccbill_id($id); 
    }
    else {
	$ccbj = sprintf("%04d%8s%3s", $m_id, $crc, substr($_SESSION["tr"], 0, 3));
	if ($_SESSION["ps"] == "1" || $_COOKIE["ps"] == "1") {
	    $ccbj[0] = chr(ord('a') + $ccbj[0]);
	}
	$ccb_id = "referer=".$ccbj;
    }
    // add custom variable to url
    switch ($b_id) {
    case 10: // CCBILL
	$url .= "&$ccb_id";
	break;
    case 21: // Segpay
	$url .= "&myID=$sv_id";
	break;
    case 24: // Epoch
	if ($m_id == 0) {
	    $url .= "&x_cust=$sv_id&reseller=a";
	}
	else {
	    $url .= "&x_cust=$sv_id&reseller=".$_SESSION['id'];
	}
	break;
    case 25: //365Bill
	$url .= "&User1=$sv_id";
	break;
    case 28:  // GXBill
	$db->query_row("SELECT login FROM billings WHERE b_id = 28");
	$url .= "&id=".$db->r["login"]."-$sv_id";
	break;
    default:
	print "curl_data(): unsupported billing (ID $b_id)";
	exit;
    }
    
    // return data
    return array($url, $c_id, $ctype, $pos, $b_id);
}

//////////////////
// start

session_start();

// restore form data
$vars = $_SESSION['vars'];

// save webmaster variables
foreach (array('id', 'ref', 'tr', 'ps') as $n) {
    // set from query
    if (isset($_GET[$n])) {
	$_SESSION[$n] = $_GET[$n];
	cookie($n, $_GET[$n]);
    }
    if (!isset($_SESSION[$n])) {
	// try from cookies
	$_SESSION[$n] = isset($_COOKIE[$n]) ? $_COOKIE[$n] : '';
    }
}


// redirect to billing url
function bill_redir($cu_id) {
    global $db, $sid, $vars;
    
    // save form data
    $_SESSION['vars'] = $vars;
    
    // option data 
    list($url, $c_id, $ctype, $pos, $b_id) = curl_data($cu_id);

    // save stats
    $db->query("SELECT * FROM jclicks WHERE day = CURRENT_DATE AND site_id = $sid AND b_id = $b_id AND type = $ctype");
    if ($db->nr() == 0) {
	$db->query("INSERT INTO jclicks (day, site_id, b_id, type, cnt) VALUES (CURRENT_DATE, $sid, $b_id, $ctype, 1)");
    }
    else {
	$db->query("UPDATE jclicks SET cnt = cnt + 1 WHERE day = CURRENT_DATE AND site_id = $sid AND b_id = $b_id AND type = $ctype");
    }
    
    // mark c_id as used
    if ($_SESSION['cascade'][$ctype] != "") {
	$cids = explode(",", $_SESSION['cascade'][$ctype]);
	if (!in_array($c_id, $cids)) {
	    array_push($cids, $c_id);
	}
    }
    else {
	$cids = array($c_id);
    }
    $_SESSION['cascade'][$ctype] = implode(",", $cids);
    
    // save last attempt type
    $_SESSION['last_ctype'] = $ctype;
    $_SESSION['last_pos'] = $pos;
    
    // redirect
    header("Location: $url");
    exit(302);
}


// try signup
if ($_SERVER['REQUEST_METHOD'] == 'POST') {
    // check option selected
    if (!isset($_POST['cu_id'])) {
	print 'ERROR : $cu_id is not set';
	exit;
    }
    $cu_id = intval($_POST['cu_id']);
    
    // check requid fields
    check_fields();
    // all fields filled
    if (count($errors) == 0) {
	bill_redir($cu_id);
    }
}


// compose pay options
$opts = array();
$types = array(0 => 'cc', 1 => 'ch', 2 => 'ph');

for ($i = 0; $i <= 2; $i++) {
    $type = $types[$i];

    // get next billing
    list($c_id, $redir) = get_c_id($i);
    
    // CC cascade end
    if ($i == 0 && $c_id == 0) {
	// CC - reset cascade and redirect to 'All CC declined page'
	$_SESSION['cascade'][0] = '';
	$_SESSION['last_ctype'] = '';
	header("Location: all_declined.html");
	exit(302);
    }
    
    // CHECK cascade end
    if ($i == 1 && $c_id == 0) {
	// check - show first bill
	$c_id = get_def_ch_id();
	$_SESSION['last_ctype'] = '';
	if ($c_id == 0) {
	    // no default check bill
	    continue;
	}
    }

    // PHONE cascade end
    if ($i == 2 && $c_id == 0) {
	// phone - show first bill
	$c_id = get_def_ph_id();
	$_SESSION['last_ctype'] = '';
	if ($c_id == 0) {
	    // no default phone bill
	    continue;
	}
    }

    // redirect to billing form
    if ($_SESSION['last_ctype'] == $i && $redir == 1 && $_SESSION['last_ctype'] != '' && $_SESSION['last_pos'] != '') {
	// get url with same pos
	$db->query_row("SELECT cu_id FROM curls WHERE c_id  = $c_id AND pos = ".addslashes($_SESSION['last_pos']));
	if ($db->nr() > 0) {
	    bill_redir($db->r["cu_id"]);
	}
    }

    // billing name
    $db->query_row("SELECT c_id, name FROM casc WHERE c_id = $c_id");
    $opts[$type]['billing'] = $db->r["name"];
    $n = 0;
    $ch = $sel = "";
    $chcu = FALSE;
    if (isset($cu_id)) {
	$db->query_row("SELECT * FROM curls WHERE c_id = $c_id AND cu_id = $cu_id ORDER BY pos ASC");
	$chcu = $db->nr() != 0;
    }
    // global trials
    $db->query_row("SELECT trial FROM sites WHERE site_id = $sid");
    $t_excl = $db->r["trial"] == "0" ? " AND trial = 0" : "";
    // webmaster trials
    $id = $_SESSION['id'];
    $m_id = get_m_id($id);
    $db->query_row("SELECT * from trials WHERE site_id = $sid AND m_id = $m_id");
    if ($db->nr() != 0) {
	$t_excl = $db->r["trial"] == "0" ? " AND trial = 0" : "";
    }

    $db->query("SELECT * FROM curls WHERE c_id = $c_id $t_excl ORDER BY pos ASC");
    while ($db->next_record()) {
	// selectd option
	if (isset($cu_id) && $chcu) {
	    $ch = $db->r["cu_id"] == $cu_id ? " checked" : "";
	    $sel = $db->r["cu_id"] == $cu_id ? " checked" : "";
	}
	else {
	    $ch = $n == 0 ? " checked" : "";
	    $sel = $n == 0 ? " selected" : "";
	}
	// for custom joins
	$opts[$type]['id'][$n] = $db->r["cu_id"];
	$opts[$type]['text'][$n] = $db->r["name"];
	// radio
	$opts[$type]['radio'][$n] = "<input type=radio name=cu_id value='".$db->r["cu_id"]."'".$ch.">".$db->r["name"];
	// button
	$opts[$type]['button'][$n] = "
<form method=POST action=".$_SERVER['REQUEST_URI'].">
<input type=hidden name=cu_id value='".$db->r["cu_id"]."'>
<input type=submit name=submit value='".$db->r["name"]."' class=bcl>
</form>
";
	$n++;
	// select
	$opts[$type]['select'] .= "<option value='".$db->r["cu_id"]."'".$sel.">".$db->r["name"]."\n";
    }
    // end select
    $opts[$type]['select'] = "<select name=cu_id>".$opts[$type]['select']."</select>";
}

// reset last attempt type
$_SESSION['last_ctype'] = '';
$_SESSION['last_pos'] = '';

// save join load stats
$db->query("SELECT * FROM jclicks WHERE day = CURRENT_DATE AND site_id = $sid AND b_id = 0 AND type = 0");
if ($db->nr() == 0) {
    $db->query("INSERT INTO jclicks (day, site_id, b_id, type, cnt) VALUES (CURRENT_DATE, $sid, 0, 0, 1)");
}
else {
    $db->query("UPDATE jclicks SET cnt = cnt + 1 WHERE day = CURRENT_DATE AND site_id = $sid AND b_id = 0 AND type = 0");
}

?>

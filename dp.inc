<?php
// $Id: dp.inc,v 1.1.2.15.2.16.2.13 2008/09/02 11:56:11 dm Exp $

$geoip_dat = "/usr/share/GeoIP/GeoIP.dat";
$gi = '';

include_once("/home/dp/misc/geoip.inc");

define("WM_COOKIE_DAYS", 30);
define("CCBILL_SESS_TIME", 10);

function get_m_id($id) {
    $file = file('/home/dp/spool/members');
    foreach ($file as $line) {
	$line = rtrim($line);
	$arr = explode('=', $line);
	if ($arr[0] == $id) {
	    return $arr[1];
	}
    }
    // try aliases file
    if (is_readable('/home/dp/spool/aliases')) {
	$file = file('/home/dp/spool/aliases');
	foreach ($file as $line) {
	    $line = rtrim($line);
	    $arr = explode('=', $line);
	    if ($arr[0] == $id) {
		return $arr[1];
	    }
	}
    }
    return "0";
}

// alias m_id -> real rev_id
function get_real_id($m_id) {
    if ($m_id != "0") {
	$file = file('/home/dp/spool/members');
	foreach ($file as $line) {
	    $line = rtrim($line);
	    $arr = explode('=', $line);
	    if ($arr[1] == $m_id) {
		return $arr[0];
	    }
	}
    }
}

function get_ccbill_id($id) {
    global $ccbill_ids;

    $ccbill_type = 0; // 0: single id, 1: multi id

    if ($ccbill_type == 0) {
	// single account variant
	if (is_readable('/home/dp/spool/ccbill')) {
	    $file = file('/home/dp/spool/ccbill');
	    foreach ($file as $line) {
		$line = rtrim($line);
		$arr = explode('=', $line);
		if ($arr[0] == $id) {
		    return $arr[1];
		}
	    }
	}
    }
    else {
	// multi account variant
	return $ccbill_ids[$id];
    }
}

function get_client_id() {
    $file = file('/home/dp/spool/ccbill.id');
    return rtrim($file[0]);
}

function clean_sessions() {

    $dir = opendir('/home/dp/spool/ips');
    if ($dir) {
	$now = time();
	while (($f = readdir($dir)) !== FALSE) {
	    $f = '/home/dp/spool/ips/'.$f;
	    if (is_file($f) && ($now - filectime($f) > CCBILL_SESS_TIME)) {
		unlink($f);
	    }
	}
	closedir($dir);
    }
}

function save_ip() {
    global $_SERVER, $_GET;
    $file = "/home/dp/spool/ips/".$_SERVER["REMOTE_ADDR"];
    $fp = fopen($file, 'w');
    fputs($fp, serialize($_GET));
    fclose($fp);
    clean_sessions();
}

function read_ip() {
    global $_SERVER, $_GET;
    $file = "/home/dp/spool/ips/".$_SERVER["REMOTE_ADDR"];
    if (file_exists($file)) {
	// delete expired
	if (time() - filectime($file) > CCBILL_SESS_TIME) {
	    unlink($file);
	    return ;
	}
	$rd = isset($_GET['rd']);
	$fp = fopen($file, 'r');
	$_GET = unserialize(fgets($fp));
	if ($rd) {
	    // prevent redirect loop
	    $_GET['rd'] = '1';
	    unset($_GET['tour']);
	}
	fclose($fp);
    }
}

function exist_ip() {
    global $_SERVER;
    $file = "/home/dp/spool/ips/".$_SERVER["REMOTE_ADDR"];
    return file_exists($file);
}

function ref_crc($ref) {

    $ref = urldecode($ref);

    $buff = array(0, 0, 0, 0);
    $i = 0;

    for ($n = 0; $n < strlen($ref); $n++) {
        $buff[$i++] += ord(substr($ref, $n, 1));
	if ($i == 4) {
	    $i = 0;
	}
    }
    $res = sprintf("%02x%02x%02x%02x", $buff[0], $buff[1], $buff[2], $buff[3]);
    return substr($res, 0, 8);
}

function is_ccbill_id($id) {
   $file = file('/home/dp/spool/ccbill');
   foreach ($file as $line) {
       $line = rtrim($line);
       $arr = explode('=', $line);
       if ($arr[0] == $id) {
           return true;
       }
   }
   return false;  
}

function check_ban_status($m_id) {
    if (is_readable("/home/dp/spool/ban.$m_id")) {
	$bf = fopen("/home/dp/spool/ban.$m_id", "r");
	$bm = fread($bf, 4096);
	fclose($bf);
	if (substr($bm, 0, 7) == "http://") {
	    // redirect to url
	    header("Location: $bm");
	    exit(302);
	}
	else {
	    if (ltrim(rtrim($bm)) != "") {
		// show ban message
		print "$bm\n";
		exit(0);
	    }
	    else {
		// redirect to default url
		$bf = fopen("/home/dp/spool/redir.ban.url", "r");
		$bm = fread($bf, 4096);
		fclose($bf);
		header("Location: $bm");
		exit(302);
	    }
	}
    }
}

function load_geo_groups() {
    $r = array("--" => "other");
    
    $ggf = '/home/dp/spool/geogroups';
    if (is_readable($ggf)) {
	$file = file($ggf);
	foreach ($file as $line) {
	    $line = rtrim($line);
	    $arr = explode('=', $line);
	    $r[$arr[0]] = $arr[1];
	}
    }
    return $r;
}

function get_country() {
    global $_GET, $_SERVER, $gi;

    if (isset($_GET['country'])) {
	return $_GET['country'];
    }
    $cntr = geoip_country_code_by_addr($gi, $_SERVER["REMOTE_ADDR"]);
    $cntr = $cntr == "" ? "--" : $cntr;
    return $cntr;
}

/*
function handle_exit_urls($m_id, $exit_urls) { 
    global $_GET;  
 
    if (isset($_GET['ex']) && isset($exit_urls[$m_id][$_GET['ex']])) { 
        $url = $exit_urls[$m_id][$_GET['ex']]; 
    } 
    print " 
<script language=JavaScript>
<!--
var exit=true;
function bye(){
  exit_url = '$url';
  if (exit && exit_url.length > 0) {
    window.open(exit_url);
  }
}
-->
</script>
"; 
}
*/

function dpmain($sid, $counter) {
    // avoid double call
    global $dpmain_init;
    if (isset($dpmain_init)) {
	return;
    }
    $dpmain_init = true;

    // exported variables
    global $_SERVER, $_GET;
    global $dpid, $dpid2, $dpid3, $sv_id;
    global $ccbj_id, $ccbj_get_id, $ccbill_ids;
    global $verotel_id, $verotel_tr_id;
    global $geoip_dat, $gi, $geo_groups;
    global $trback_exit_url;
    
    $self = $_SERVER["PHP_SELF"];
    
    // load site config
    $jpages = array();
    $cfg_file = "/home/dp/spool/site.$sid";
    if (!is_readable($cfg_file)) {
	print "can't open config file '$cfg_file'\n<br>";
	print "check config directory /home/dp/spool or site ID parameter at dpmain() call\n";
	exit;
    }
    $ccbill_ids = $jpages = $designs = $exit_urls = $dlink = $c_enter = array();
    $custom_joins = $custom_designs = $geogroups = $geo_desings = $geo_jpages = array();
    $fp = fopen($cfg_file, "r");
    if (!$fp) exit;
    while (!feof($fp)) {
	$l = fgets($fp);
	list($var, $val) = explode(" = ", rtrim($l));
	$val = substr($val, 1, strlen($val)-2);
	// index page
	if ($var == "index") {
	    $index = $val;
	}
	// join page
	if ($var == "mainjpage") {
	    $mainjpage = $val;
	}
	// additional joins
	if ($var == "jpages") {
	    array_push($jpages, $val);
	}
	// designs
	if ($var == "design") {
	    list($dnum, $dpage) = explode(" ", $val);
	    $designs[$dpage] = $dnum;
	}
	// trafficback
	if ($var == "exit") {
	    list($mid, $num, $url) = explode(" ", $val);
	    $exit_urls[$mid][$num] = $url;
	}
	// custom join pages
	if ($var == "jpage") {
	    list($mid, $cjpage) = explode(" ", $val);
	    $custom_joins[$mid] = $cjpage;
	}
	// custom design
	if ($var == "des") {
	    list($mid, $cdes) = explode(" ", $val);
	    $custom_designs[$mid] = $cdes;
	}
	// direct links
	if ($var == "dlink") {
	    list($dom, $mid, $ex, $cons, $page) = explode(" ", $val);
	    $dlinks[$dom] = array($mid, $ex, $cons, $page);
	}
	// geoip redirect
	if ($var == "c_enter") {
	    list($mid, $cntr, $rd_url) = explode(" ", $val);
	    $c_enter[$cntr][$mid] = $rd_url;
	}
	// ccbill ids
	if ($var == "ccb") {
	    list($mid, $num) = explode(" ", $val);
	    $ccbill_ids[$mid] = $num;
	}
    }
    fclose($fp);
    // geogroups settings
    $cfg_file = "/home/dp/spool/geo.$sid";
    if (is_readable($cfg_file)) {
	$fp = fopen($cfg_file, "r");
	while (!feof($fp)) {
	    $l = fgets($fp);
	    list($var, $val) = explode(" = ", rtrim($l));
	    $val = substr($val, 1, strlen($val)-2);
	    if ($var == "join") {
		list($cntr, $cjpage) = explode(" ", $val);
		$geo_jpages[$cntr] = $cjpage;
	    }
	    if ($var == "design") {
		list($cntr, $cdes) = explode(" ", $val);
		$geo_designs[$cntr] = $cdes;
	    }
	}
    }
    $geo_groups = load_geo_groups();

    // init GeoIP
    if (file_exists($geoip_dat)) {
	$gi = geoip_open($geoip_dat, GEOIP_STANDARD);
    }
    else {
	print "can't open GeoIP data file '$geoip_dat'\n";
	exit;
    }
    
    // set is_index
    $is_index = $counter == 'index';

    read_ip();
    
    // direct links
    if ($is_index) { 
        $r = str_replace("http://", "", $_SERVER['HTTP_REFERER']); 
        $r = preg_replace("/\/.*/", "", $r); 
        if (!isset($dlinks[$r])) { 
            $r = preg_replace("/^www\./", "", $r); 
        } 
        if (isset($dlinks[$r])) { 
            // domain found 
            list($id, $ex, $cons, $page) = $dlinks[$r]; 
            if (!isset($_GET["id"])) $_GET["id"] = $id; 
            if (!isset($_GET["ex"])) $_GET["ex"] = $ex; 
            if (!isset($_GET["tour"])) $_GET["tour"] = $page; 
        } 
    } 

    // ids
    if (isset($_GET["id"])) {
	$id = $_GET["id"];
    }
    else {
	// try from cookie
	if (isset($_COOKIE["id"]) && $self == $mainjpage || in_array($self, $jpages)) {
	    $id = $_COOKIE["id"];
	}
	if (isset($_COOKIE["ref"]) && $self == $mainjpage || in_array($self, $jpages)) {
	    $join_c_ref = $_COOKIE["ref"];
	}
    }
        
    $m_id = get_m_id($id);

    // ban status
    check_ban_status($m_id);
    
    // back from alias
    $real_id = get_real_id($m_id);
    if (isset($real_id)) {
	$id = $real_id;
    }

    // referer
    $ref = isset($_SERVER["HTTP_REFERER"]) ? urlencode($_SERVER["HTTP_REFERER"]) : "bookmark";
    if (!$is_index && isset($_GET['ref'])) {
	$ref = urlencode($_GET['ref']);
    }

    // traffic back
    if (isset($_GET['ex']) && isset($exit_urls[$m_id][$_GET['ex']])) { 
        $trback_exit_url = $exit_urls[$m_id][$_GET['ex']]; 
    } 

    // handle join pages
    if ($self == $mainjpage || in_array($self, $jpages)) {
        // banners tracking - restore from cookie
        if (!isset($_GET["tr"])) {
	    $_GET["tr"] = $_COOKIE["tr"];
	}

	// PPS links
	if (!isset($_GET['ps']) && isset($_COOKIE['ps'])) {
	  //$_GET["ps"] = $_COOKIE["ps"];
	}

	// compose tags

	// referer checksum
        $crc = ref_crc(isset($_GET['ref']) ? $_GET['ref'] : $join_c_ref);
	// common 
	$sv_id = $id."_".$crc."_";
	if ($_GET['tr'] != "") {
	    $sv_id .= $_GET['tr'];
	}
	$sv_id .= "_";
	if ($_GET['ps'] != "") {
	    $sv_id .= "ps";
	}

	// verotel
	$verotel_id = substr($crc, 0, 8).$id;
	$verotel_tr_id = $_GET['tr'];
	// CCBILL
	   if (get_ccbill_id($id) != "") {
		$ccbj_id = "<input type='hidden' name='ccbill_referer' value='".get_ccbill_id($id)."'>";
		$ccbj_get_id = "&ccbill_referer=".get_ccbill_id($id); 
	    }
	    else {
		$ccbj = sprintf("%04d%8s%-3s", $m_id, $crc, substr($_GET["tr"], 0, 3));
		if ($_GET["ps"] == "1" || $_COOKIE["ps"] == "1") {
		    $ccbj[0] = chr(ord('a') + $ccbj[0]);
		}
		$ccbj_id = "<input type='hidden' name='referer' value='".$ccbj."'>";
		$ccbj_get_id = "&referer=".$ccbj;
	    }
	// counter name
	$log .= "$sid|$m_id|$counter\n";
	$fp = fopen('/home/dp/spool/stats', 'a');
	fwrite($fp, $log);
	fclose($fp);
	// compose %id tag
	if ($id != "") {
	    $dpid = "?id=$id&ref=$ref";
	    $dpid2 = "&id=$id&ref=$ref";
	    $dpid3 = "id=$id&ref=$ref&";
	    if ($_GET['ex'] != "") { 
		// exit urls 
		$dpid .= "&ex=".$_GET['ex']; 
		$dpid2 .= "&ex=".$_GET['ex']; 
		$dpid3 .= "ex=".$_GET['ex']."&"; 
	    } 
            if ($_GET['tr'] != "") {
                // banners tracking
                $dpid .= "&tr=".$_GET['tr'];
                $dpid2 .= "&tr=".$_GET['tr'];
                $dpid3 .= "tr=".$_GET['tr']."&";
            }
	    if ($_GET['ps'] != "") {
		// persignup links
		$dpid .= "&ps=".$_GET['ps'];
		$dpid2 .= "&ps=".$_GET['ps'];
		$dpid3 .= "ps=".$_GET['ps']."&";
	    }
	}
	// trafficback 
//	handle_exit_urls($m_id, $exit_urls);
	// geo_groups join pages
	$cntr = get_country();
	if ($self == $mainjpage && isset($geo_jpages[$cntr])) {
	    $jp = getcwd().$geo_jpages[$cntr];
	    if (file_exists($jp)) {
		include($jp);
	    }
	    else {
		print "can't find geo join page '$jp'\n";
	    }
	    exit;
	}
	// custom join page
	if ($self == $mainjpage && isset($custom_joins[$m_id])) {
	    $jp = getcwd().$custom_joins[$m_id];
	    if (file_exists($jp)) {
		include($jp);
	    }
	    else {
		print "can't find custom join page '$jp'\n";
	    }
	    exit;
	}
	return;
    }
        
    // ccbill data
    $ccbill_id = get_ccbill_id($id);
    $client_id = get_client_id();


    // ccbill redirect
    if ($is_index && $ccbill_id != "" && !exist_ip()) {
	save_ip();
	$url = "http://refer.ccbill.com/cgi-bin/clicks.cgi?CA=".$client_id."-0000&PA=".$ccbill_id."&HTML=http://".$_SERVER["HTTP_HOST"].$_SERVER["REQUEST_URI"];
	header("Location: $url");
	exit(302);
    }

    $log = "";
    
    // country
    unset($geo_rd_url);
    $cntr = get_country();
    if (isset($c_enter[$cntr][$m_id])) {
	$geo_rd_url = $c_enter[$cntr][$m_id];
    }
    else {
	// try direct account settings
	if (isset($c_enter[$cntr]["0"])) {
	    $geo_rd_url = $c_enter[$cntr]["0"];
	}
    }

    // save stats
    if ($is_index) {
        $log .= "$sid|$m_id|cntr|$cntr\n";
    }
    if ($is_index && !isset($geo_rd_url)) {
	$log .= "$sid|$m_id|raw\n";
	if (!isset($_COOKIE["uniq"])) {
	    $log .= "$sid|$m_id|uniq\n";
	}
	
	if (isset($_SERVER["HTTP_REFERER"])) {
	    $r = $_SERVER["HTTP_REFERER"];
	    $sr_crc = ref_crc($_SERVER['HTTP_REFERER']);
	    $log .= "$sid|$m_id|sref|{$_SERVER['HTTP_REFERER']}|$sr_crc\n";
	}
	else {
	    $log .= "$sid|$m_id|ref|bookmark\n";
	}

	// banners tracking
	if (isset($_GET["tr"])) {
	    $log .= "$sid|$m_id|tr|".$_GET["tr"]."\n";
	    // blogs stats
	    if (isset($_GET["rss"])) {
		// blog click
		$log .= "$sid|$m_id|bl|".$_GET["tr"]."\n";
		// blog referer
		$r = isset($_SERVER["HTTP_REFERER"]) ? $_SERVER["HTTP_REFERER"] : "bookmark";
		$sr_crc = ref_crc($r);
		$log .= "$sid|$m_id|blref|$r|$sr_crc\n";
	    }
	}

	// FHG tracking
	//if (isset($_GET["port"]) && isset($_GET["pp"])) { // &port= - cl-cash local hack ?
	if (isset($_GET["p"]) && isset($_GET["pp"])) {
	    $log .= "$sid|$m_id|fhg|".$_GET["p"]."|".$_GET["pp"]."\n";
	}

	// proxy
	foreach (array("HTTP_X_FORWARDED_FOR", "HTTP_VIA", "HTTP_CLIENT_IP") as $header) {
	    if (isset($_SERVER[$header])) {
		$log .= "$sid|$m_id|proxy\n";
		break;
	    }
	}
	
	// set cookies
	setcookie("uniq", '1', time()+(60*60*24));
	if ($m_id != "0") {
	    setcookie("id", $id, time()+(60*60*24*WM_COOKIE_DAYS));
	}
	if (isset($_SERVER["HTTP_REFERER"])) {
	    setcookie("ref", $_SERVER["HTTP_REFERER"], time()+(60*60*24*WM_COOKIE_DAYS));
	}
	if (isset($_GET["tr"])) {
	    setcookie("tr", $_GET["tr"], time()+(60*60*24*30));
	}
	// persignup
	setcookie("ps", isset($_GET["ps"]) ? "1" : "0", time()+(60*60*24*WM_COOKIE_DAYS));
    }
    // counter name
    $log .= "$sid|$m_id|$counter\n";
    // save stats
    $fp = fopen('/home/dp/spool/stats', 'a');
    fwrite($fp, $log);
    fclose($fp);

    // GeoIP redirect
    if (isset($geo_rd_url) && $geo_rd_url != "-" && !isset($_GET['wmlink'])) {
	if (strlen($geo_rd_url) > 7 && substr($geo_rd_url, 0, 7) == "http://") {
	    // redirect
	    header("Location: $geo_rd_url");
	    exit(302);
	}
	else {
	    // show text
	    print "$geo_rd_url";
	    exit;
	}
    }

    
    // &tour= param
    unset($page);
    if (isset($_GET["tour"])) $page = urldecode($_GET["tour"]);
    if ($page != "" && !isset($_GET["rd"])) {
	$c = !strpos($page, "?") ? "?" : "&";
	if ($id != "") {
	    $url = $page.$c."id=$id&ref=$ref&rd=1";
	}
	else {
	    $url = $page.$c."ref=$ref&rd=1";
	}
	if ($_GET['ex'] != "") {
	    $url .= "&ex=".$_GET['ex'];
	}
	header("Location: $url");
	exit(302);
    }
    
    // design num
    unset($des);
    if (isset($_GET['d'])) {
	$des = $_GET['d'];
    }
    else {
	// no &d= - try define design by uri
	if (isset($designs[$_SERVER["SCRIPT_NAME"]])) {
	    $des = $designs[$_SERVER["SCRIPT_NAME"]];
	}
    }
    
    // compose %id tag
    if ($m_id != "0") {
	$dpid = "?id=$id&ref=$ref";
	$dpid2 = "&id=$id&ref=$ref";
	$dpid3 = "id=$id&ref=$ref&";
	if ($_GET['ex'] != "") { 
            // exit urls 
            $dpid .= "&ex=".$_GET['ex']; 
            $dpid2 .= "&ex=".$_GET['ex']; 
            $dpid3 .= "ex=".$_GET['ex']."&"; 
        } 
	if ($_GET['tr'] != "") {
	    // banners tracking
	    $dpid .= "&tr=".$_GET['tr'];
	    $dpid2 .= "&tr=".$_GET['tr'];
	    $dpid3 .= "tr=".$_GET['tr']."&";
	}
	if ($_GET['ps'] != "") {
	    // persignup links
	    $dpid .= "&ps=".$_GET['ps'];
	    $dpid2 .= "&ps=".$_GET['ps'];
	    $dpid3 .= "ps=".$_GET['ps']."&";
	}
	if (isset($des) && $des != "0") {
	    // add design number for joins
	    $dpid .= "&d=".$des;
	    $dpid2 .= "&d=".$des;
	    $dpid3 .= "d=".$des."&";
	}
    }

    // geogroups redirect
    $cntr = get_country();
    if ($is_index && isset($geo_designs[$cntr])) {
	$page = $geo_designs[$cntr];
	$url = $dpid != "" ? "$page$dpid" : "$page";
	header("Location: $url");
	exit(302);
    }

    // custom designs redirect
    if ($is_index && isset($custom_designs[$m_id]) ) {
	header("Location: ".$custom_designs[$m_id].$dpid);
	exit(302);
    }
    
    // trafficback 
//    handle_exit_urls($m_id, $exit_urls);  
}


function geo_include($group, $file) {
    global $geo_groups;

    $cntr = get_country();
    if ($geo_groups[$cntr] == $group) {
	// country match group
	if (is_readable($file)) {
	    include($file);
	}
	else {
	    print "\n<!-- geo_include : can't find file '$file' -->\n";
	}
    }
}


// FHG procedure
function dpfhg($sid) {
    global $_SERVER, $_GET;
    global $fhgid, $fhgtourid;

    $self = $_SERVER["REQUEST_URI"];
    if (($n = strpos($self, "?")) !== FALSE ) {
	$self = substr($self, 0, $n);
    }
    // webmaster
    if (isset($_GET["id"])) {
	$id = $_GET["id"];
    }
    else {
	// try cookie
	if (isset($_COOKIE["id"])) {
	    $id = $_COOKIE["id"];
	}
    }
    $m_id = get_m_id($id);

    // ban status
    check_ban_status($m_id);

    // save stats
    if (!isset($_GET['pp'])) {
	$log = "$sid|$m_id|raw|$self\n";
	if (!isset($_COOKIE["uniq"])) {
	    $log .= "$sid|$m_id|uniq|$self\n";
	}
	if (isset($_SERVER["HTTP_REFERER"])) {
	    $sr_crc = ref_crc($_SERVER['HTTP_REFERER']);
	    $log .= "$sid|$m_id|ref|{$_SERVER['HTTP_REFERER']}|$self\n";
	}
	else {
	    $log .= "$sid|$m_id|ref|bookmark|$self\n";
	}
	$fp = fopen('/home/dp/spool/stats', 'a');
	fwrite($fp, $log);
	fclose($fp);
    }

    // set uniq cookie
    setcookie("uniq", '1', time()+(60*60*24));
    if ($m_id != "0") {
# setcookie("id", $id, time()+(60*60*24*WM_COOKIE_DAYS));
    }

    // banners tracking
    if (isset($_GET['tr'])) {
	$tr = "&tr=".$_GET['tr'];
    }

    // persignup links
    if (isset($_GET['ps'])) {
        $ps = "&ps=".$_GET['ps'];
    }

    // FHS pages
    if (isset($_GET['pp'])) {
	$self = $_GET['pp'];
    }
    
    // compose $fhgid
    if ($id != "") {
        $fhgid = "?id=$id&p=$sid&pp=$self$tr$ps";
        $fhgtourid = "&id=$id&p=$sid&pp=$self$tr$ps";
    }
    else {
        $fhgid = "?p=$sid&pp=$self$tr$ps";
        $fhgtourid = "&p=$sid&pp=$self$tr$ps";
    }

}

?>

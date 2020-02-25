<?php
// $Id: db.inc,v 1.1.2.3.4.6.2.2 2006/11/08 16:14:41 dm Exp $

// class from PHPLib

class DB_Sql {
  
  /* public: connection parameters */
  var $Host     = "";
  var $Database = "";
  var $User     = "";
  var $Password = "";

  /* public: configuration parameters */
  var $Auto_Free     = 0;     ## Set to 1 for automatic mysqli_free_result()
  var $Debug         = 0;     ## Set to 1 for debugging messages.
  var $Halt_On_Error = "yes"; ## "yes" (halt with message), "no" (ignore errors quietly), "report" (ignore errror, but spit a warning)
  var $Seq_Table     = "db_sequence";

  /* public: result array and current row number */
  var $Record   = array();
  var $Row;

  /* public: current error number and error text */
  var $Errno    = 0;
  var $Error    = "";

  /* public: this is an api revision, not a CVS revision. */
  var $type     = "mysql";
  var $revision = "1.2";

  /* private: link and query handles */
  var $Link_ID  = 0;
  var $Query_ID = 0;
  
  /* public: constructor */
  function DB_Sql($query = "") {
      $this->query($query);
  }

  /* public: some trivial reporting */
  function link_id() {
    return $this->Link_ID;
  }

  function query_id() {
    return $this->Query_ID;
  }

  /* public: connection management */
  function connect($Database = "", $Host = "", $User = "", $Password = "") {
    /* Handle defaults */
    if ("" == $Database)
      $Database = $this->Database;
    if ("" == $Host)
      $Host     = $this->Host;
    if ("" == $User)
      $User     = $this->User;
    if ("" == $Password)
      $Password = $this->Password;
      
    /* establish connection, select database */
    if ( 0 === $this->Link_ID ) {
    
      $this->Link_ID=mysqli_connect($Host, $User, $Password);
      if (!$this->Link_ID) {
        $this->halt("connect($Host, $User, \$Password) failed.");
        return 0;
      }

      if (!mysqli_select_db($this->Link_ID, $Database)) {
        $this->halt("cannot use database ".$this->Database);
        return 0;
      }
    }
    
    return $this->Link_ID;
  }

  /* public: discard the query result */
  function free() {
      mysqli_free_result($this->Query_ID);
      $this->Query_ID = 0;
  }

  /* public: perform a query */
  function query($Query_String) {
    /* No empty queries, please, since PHP4 chokes on them. */
    if ($Query_String == "")
      /* The empty query string is passed on from the constructor,
       * when calling the class without a query, e.g. in situations
       * like these: '$db = new DB_Sql_Subclass;'
       */
      return 0;

    if (!$this->connect()) {
      return 0; /* we already complained in connect() about that. */
    };

    # New query, discard previous result.
    if ($this->Query_ID) {
      $this->free();
    }

    if ($this->Debug)
      printf("Debug: query = %s<br>\n", $Query_String);

    $this->Query_ID = mysqli_query($this->Link_ID, $Query_String);
    $this->Row   = 0;
    $this->Errno = mysqli_errno();
    $this->Error = mysqli_error();
    if (!$this->Query_ID) {
      $this->halt("Invalid SQL: ".$Query_String);
    }

    # Will return nada if it fails. That\'s fine.
    return $this->Query_ID;
  }

  /* public: walk result set */
  function next_record() {
    if (!$this->Query_ID) {
      $this->halt("next_record called with no query pending.");
      return 0;
    }

    $this->Record = mysqli_fetch_array($this->Query_ID);
    $this->Row   += 1;
    $this->Errno  = mysqli_errno();
    $this->Error  = mysqli_error();

    $stat = is_array($this->Record);
    if (!$stat && $this->Auto_Free) {
      $this->free();
    }
//    return $stat;
    return $this->Record;
  }

  /* public: position in result set */
  function seek($pos = 0) {
    $status = mysqli_data_seek($this->Query_ID, $pos);
    if ($status)
      $this->Row = $pos;
    else {
      $this->halt("seek($pos) failed: result has ".$this->num_rows()." rows");

      /* half assed attempt to save the day, 
       * but do not consider this documented or even
       * desireable behaviour.
       */
      mysqli_data_seek($this->Query_ID, $this->num_rows());
      $this->Row = $this->num_rows;
      return 0;
    }

    return 1;
  }

  /* public: table locking */
  function lock($table, $mode="write") {
    $this->connect();
    
    $query="lock tables ";
    if (is_array($table)) {
      while (list($key,$value)=each($table)) {
        if ($key=="read" && $key!=0) {
          $query.="$value read, ";
        } else {
          $query.="$value $mode, ";
        }
      }
      $query=substr($query,0,-2);
    } else {
      $query.="$table $mode";
    }
    $res = mysqli_query($query, $this->Link_ID);
    if (!$res) {
      $this->halt("lock($table, $mode) failed.");
      return 0;
    }
    return $res;
  }
  
  function unlock() {
    $this->connect();

    $res = mysqli_query("unlock tables");
    if (!$res) {
      $this->halt("unlock() failed.");
      return 0;
    }
    return $res;
  }


  /* public: evaluate the result (size, width) */
  function affected_rows() {
    return mysqli_affected_rows($this->Link_ID);
  }

  function num_rows() {
    return mysqli_num_rows($this->Query_ID);
  }

  function num_fields() {
    return mysqli_num_fields($this->Query_ID);
  }

  /* public: shorthand notation */
  function nr() {
    return $this->num_rows();
  }

  function np() {
    print $this->num_rows();
  }

  function f($Name) {
    return $this->Record[$Name];
  }

  function p($Name) {
    print $this->Record[$Name];
  }

  /* public: sequence numbers */
  function nextid($seq_name) {
    $this->connect();
    
    if ($this->lock($this->Seq_Table)) {
      /* get sequence number (locked) and increment */
      $q  = sprintf("select nextid from %s where seq_name = '%s'",
                $this->Seq_Table,
                $seq_name);
      $id  = mysqli_query($q, $this->Link_ID);
      $res = mysqli_fetch_array($id);
      
      /* No current value, make one */
      if (!is_array($res)) {
        $currentid = 0;
        $q = sprintf("insert into %s values('%s', %s)",
                 $this->Seq_Table,
                 $seq_name,
                 $currentid);
        $id = mysqli_query($q, $this->Link_ID);
      } else {
        $currentid = $res["nextid"];
      }
      $nextid = $currentid + 1;
      $q = sprintf("update %s set nextid = '%s' where seq_name = '%s'",
               $this->Seq_Table,
               $nextid,
               $seq_name);
      $id = mysqli_query($q, $this->Link_ID);
      $this->unlock();
    } else {
      $this->halt("cannot lock ".$this->Seq_Table." - has it been created?");
      return 0;
    }
    return $nextid;
  }

  /* public: return table metadata */
  function metadata($table='',$full=false) {
    $count = 0;
    $id    = 0;
    $res   = array();

    /*
     * Due to compatibility problems with Table we changed the behavior
     * of metadata();
     * depending on $full, metadata returns the following values:
     *
     * - full is false (default):
     * $result[]:
     *   [0]["table"]  table name
     *   [0]["name"]   field name
     *   [0]["type"]   field type
     *   [0]["len"]    field length
     *   [0]["flags"]  field flags
     *
     * - full is true
     * $result[]:
     *   ["num_fields"] number of metadata records
     *   [0]["table"]  table name
     *   [0]["name"]   field name
     *   [0]["type"]   field type
     *   [0]["len"]    field length
     *   [0]["flags"]  field flags
     *   ["meta"][field name]  index of field named "field name"
     *   The last one is used, if you have a field name, but no index.
     *   Test:  if (isset($result['meta']['myfield'])) { ...
     */

    // if no $table specified, assume that we are working with a query
    // result
    if ($table) {
      $this->connect();
      $id = mysqli_list_fields($this->Database, $table);
      if (!$id)
        $this->halt("Metadata query failed.");
    } else {
      $id = $this->Query_ID; 
      if (!$id)
        $this->halt("No query specified.");
    }
 
    $count = mysqli_num_fields($id);

    // made this IF due to performance (one if is faster than $count if's)
    if (!$full) {
      for ($i=0; $i<$count; $i++) {
        $res[$i]["table"] = mysqli_field_table ($id, $i);
        $res[$i]["name"]  = mysqli_field_name  ($id, $i);
        $res[$i]["type"]  = mysqli_field_type  ($id, $i);
        $res[$i]["len"]   = mysqli_field_len   ($id, $i);
        $res[$i]["flags"] = mysqli_field_flags ($id, $i);
      }
    } else { // full
      $res["num_fields"]= $count;
    
      for ($i=0; $i<$count; $i++) {
        $res[$i]["table"] = mysqli_field_table ($id, $i);
        $res[$i]["name"]  = mysqli_field_name  ($id, $i);
        $res[$i]["type"]  = mysqli_field_type  ($id, $i);
        $res[$i]["len"]   = mysqli_field_len   ($id, $i);
        $res[$i]["flags"] = mysqli_field_flags ($id, $i);
        $res["meta"][$res[$i]["name"]] = $i;
      }
    }
    
    // free the result only if we were called on a table
    if ($table) mysqli_free_result($id);
    return $res;
  }

  /* private: error handling */
  function halt($msg) {
    $this->Error = mysqli_error($this->Link_ID);
    $this->Errno = mysqli_errno($this->Link_ID);
    if ($this->Halt_On_Error == "no")
      return;

    $this->haltmsg($msg);

    if ($this->Halt_On_Error != "report")
      die("Session halted.");
  }

  function haltmsg($msg) {
    printf("</td></tr></table><b>Database error:</b> %s<br>\n", $msg);
    printf("<b>MySQL Error</b>: %s (%s)<br>\n",
      $this->Errno,
      $this->Error);
  }

  function table_names() {
    $this->query("SHOW TABLES");
    $i=0;
    while ($info=mysqli_fetch_row($this->Query_ID))
     {
      $return[$i]["table_name"]= $info[0];
      $return[$i]["tablespace_name"]=$this->Database;
      $return[$i]["database"]=$this->Database;
      $i++;
     }
   return $return;
  }
}


class PartnerDB extends DB_Sql {
    
    var $classname = "PartnerDB";

    var $parse_config = true;
    var $config_file  = "/etc/partner.conf";

    // default
    var $Host     = "";
    var $Database = "partner";
    var $User     = "partner";
    var $Password = "topsecret";
    var $r;

    function PartnerDB() {
	// load settings from config file
	if ($this->parse_config) {
	    if (!($f = @fopen($this->config_file, "r"))) {
		print "<pre>PartnerDB(): can't open config file '$this->config_file'</pre>\n";
		exit;
	    }
	    while (!feof($f)) {
		$l = fgets($f);
		if (preg_match('/dbname\s*=\s*[\'"]?(\S+?)[\'"]?;/', $l, $res)) {
		    $this->Database = $res[1];
		}
		if (preg_match('/dbuser\s*=\s*[\'"]?(\S+?)[\'"]?;/', $l, $res)) {
		    $this->User = $res[1];
		}
		if (preg_match('/dbpassword\s*=\s*[\'"]?(\S+?)[\'"]?;/', $l, $res)) {
		    $this->Password = $res[1];
		}
		if (preg_match('/dbhost\s*=\s*[\'"]?(\S+?)[\'"]?;/', $l, $res)) {
		    $this->Host = $res[1];
		}
	    }
	    fclose($f);
	}
	// internal setup
	$this->r =& $this->Record;
    }
    
    function haltmsg($msg) {
	printf("<b>Database error:</b> %s<br>\n", $msg);
	printf("<b>Mysql Error</b>: %s (%s)<br>\n",
	       $this->Errno, $this->Error);
    }

    function query_fetch($sql) {
	$ret = $this->query($sql);
	$this->next_record();
	return $ret;
    }

    function query_fetch_col($sql, $col) {
	$this->query($sql);
	$this->next_record();
	return $this->Record[$col];
    }

    function query_row($sql) {
	$this->query($sql);
	if ($this->num_rows() == 0) {
	    return array();
	}
	$this->next_record();
	$row = array();
	foreach ($this->Record as $key => $col) {
	    if (is_numeric($key)) {
		array_push($row, $col);
	    }
	}
	return $row;
    }

    function row($sql) {
	return $this->query_fetch_row($sql);
    }
}

$db = new PartnerDB();

function table_up($table) {

    global $db;

    $days = 500;

    $ts = time();

    for ($i = 0; $i < $days; $i++) {
	$date_n = date("Y-m-d", $ts-($i*60*60*24));
	$date_p = date("Y-m-d", $ts-(($i+1)*60*60*24));
	$sql = "UPDATE $table SET day = '$date_n' WHERE day = '$date_p'";
	$db->query($sql);
    }
}


function table_down($table) {

    global $db;

    $days = 500;

    $ts = time();

    for ($i = $days; $i > 0; $i--) {
	$date_p = date("Y-m-d", $ts-($i*60*60*24));
	$date_n = date("Y-m-d", $ts-(($i+1)*60*60*24));
	$sql = "UPDATE $table SET day = '$date_n' WHERE day = '$date_p'";
	$db->query($sql);
    }
}

function all_up() {


    $tables = array(
	"clicks",
	"nclicks",
	"cclicks",
	"referers",
	"tx",
	"traf",
	"ips",
	"uas",
	"prog_referers",
	"pclicks",
	"p_stat",
	"p_referers"
	);

    foreach ($tables as $t) {
        print "Process $t\n";
	table_up($t);
    }
}

function prog_referers($index_page, $domain) {

    global $db, $_SERVER, $_GET;

    $HTTP_REFERER = $_SERVER['HTTP_REFERER'];
    $m_id = 0;
    $wm_id = addslashes($_GET['wm']);
    if ($wm_id != '') {
	$db->query_row("SELECT m_id FROM members WHERE rev_id = '$wm_id'");
	if ($db->nr() != 0) {
	    $m_id = $db->r["m_id"];
	}
    }

    $ref = addslashes($HTTP_REFERER == '' ? 'bookmark' : $HTTP_REFERER);
    $db->query("
SELECT day
FROM prog_referers
WHERE day = CURRENT_DATE AND m_id = $m_id AND referer = '$ref'
");
    if ($db->nr() == 0) {
	$db->query("
INSERT INTO prog_referers (day, m_id, referer, cnt)
VALUES (CURRENT_DATE, $m_id, '$ref', 1)
");
    }
    else {
	$db->query("
UPDATE prog_referers
SET cnt = cnt + 1
WHERE day = CURRENT_DATE AND m_id = $m_id AND referer = '$ref'
");
    }

    // include main page
    if (isset($_GET['wm'])) {
	setcookie ("wm",  $_GET['wm'], time()+(60*60*24*30), // one month 
		   "/", ".$domain"); 
    }
    setcookie ("wmref", $_SERVER['HTTP_REFERER'], time()+(60*60*24*30), "/", ".$domain"); 
    if ($index_page != "") {
        virtual("$index_page");
    }
}

function external_news_block() {

    global $db;

    $db->query_row("SELECT value FROM config WHERE name = 'external_news' AND m_id = 0");
    if ($db->nr() != 0) {
	return $db->r["value"];
    }
    else {
	return "External news block not set, check Settings / Edit news menu at admin area";
    }
}

?>

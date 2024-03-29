<?php
require_once "Utility.class.inc";
require_once "NDB_Form.class.inc";

/**
 * Conflict resolver
 * @package main
 */
class NDB_Form_conflicts_resolve extends NDB_Form
{
    
    function _hasAccess()
    {
        
        // create user object
        $user =& User::singleton();
        if(Utility::isErrorX($user)) {
            return PEAR::raiseError("User Error: ".$user->getMessage());
        }

        return ($user->hasPermission('conflict_resolver'));
    }

    function _getDefaults()
    {
  		$DB =& Database::singleton();
        //Get the default values
        $value_results = $DB->pselect("SELECT ParameterTypeID, Value 
            FROM parameter_candidate WHERE CandID=:CaID",
            array('CaID' => $this->identifier)
        );
        $defaults = array();
        foreach($value_results AS $vresult){
        	$defaults["PTID_".$vresult['ParameterTypeID']]=$vresult['Value'];
        }
        if(isset($_REQUEST['site'])) {
            $defaults['site'] = $_REQUEST['site'];
        }
        if(isset($_REQUEST['visit'])) {
            $defaults['visit'] = $_REQUEST['visit'];
        }
        if(isset($_REQUEST['Instrument'])) {
            $defaults['Instrument'] = $_REQUEST['Instrument'];
        }
        if(isset($_REQUEST['CandID'])) {
            $defaults['CandID'] = $_REQUEST['CandID'];
        }
        if(isset($_REQUEST['PSCID'])) {
            $defaults['PSCID'] = $_REQUEST['PSCID'];
        }
        return $defaults;
    }

    function _process($values)
    {
        if(!is_array($values) || count($values) ==0) {
        	return true;
        }
        
        $DB =& Database::singleton();
        
        $user =& User::singleton();
           if(Utility::isErrorX($user)) {
             return PEAR::raiseError("User Error: ".$user->getMessage());
           }


        foreach($values AS $key=>$val) {
            if($val != 'none') {
                $hash = substr($key, 9);
                $row = $DB->pselectRow(
                    "SELECT * FROM conflicts_unresolved 
                    WHERE MD5(CONCAT_WS(':',TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2)) = :hash", 
                    array('hash' => $hash)
                );
                if(!empty($row)) {
                   
                    // insert into conflicts_resolved
                    $user1 = $DB->pselectOne("SELECT UserID FROM $row[TableName] WHERE CommentID=:CID",
                        array('CID' => $row['CommentId1'])
                    );
                    $user2 = $DB->pselectOne("SELECT UserID FROM $row[TableName] WHERE CommentID=:CID",
                        array('CID' => $row['CommentId2'])
                    );
                    $resolutionLog=array(
                        'UserID' => $user->getUsername(), 
                        // 'ResolutionTimestamp'=>null, 
                        'User1'=>$user1, 
                        'User2'=>$user2, 
                        'TableName'=>$row['TableName'], 
                        'ExtraKeyColumn'=>$row['ExtraKeyColumn'], 
                        'ExtraKey1'=>$row['ExtraKey1'], 
                        'ExtraKey2'=>$row['ExtraKey2'], 
                        'FieldName'=>$row['FieldName'], 
                        'CommentId1'=>$row['CommentId1'], 
                        'CommentId2'=>$row['CommentId2'], 
                        'OldValue1'=>$row['Value1'], 
                        'OldValue2'=>$row['Value2'], 
                        'NewValue'=>$val,
                        'ConflictID'=>$row['ConflictID']
                    );

                    $DB->insert('conflicts_resolved', $resolutionLog);

                    // update the source tables
                    $setArray = array($row['FieldName'] => ($val == '1' ? $row['Value1'] : $row['Value2']));
                    $whereArray1 = array('CommentID'=>$row['CommentId1']);
                    $whereArray2 = array('CommentID'=>$row['CommentId2']);
                    if($row['ExtraKeyColumn'] != null) {
                        $whereArray1[$row['ExtraKeyColumn']] = $row['ExtraKey1'];
                        $whereArray2[$row['ExtraKeyColumn']] = $row['ExtraKey2'];
                    }
                    $DB->update($row['TableName'], $setArray, $whereArray1);
                    $DB->update($row['TableName'], $setArray, $whereArray2);

                    // delete from conflics_unresolved
                    $DB->run("DELETE FROM conflicts_unresolved WHERE MD5(CONCAT_WS(':',TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2)) = '$hash'");

                    // Load the instrument and save it to force the score 
                    // function to run.
                    // Wrapped in a try/catch block in case the table doesn't have its own
                    // instrument (ie. figs_year3_relative)
                    $TableName = $row['TableName'];
                    $Instrument = NDB_BVL_Instrument::factory($TableName, $row['CommentId1'], '', false);
                    if($Instrument != null) {
                        $Instrument->score();
                        // Score runs the update statements to update the scored columns. There's no need to save,
                        // (and save() doesn't work) because it calls $this->form->getSubmittedValues(), and at
                        // this point we're submitting the conflict resolver, not the instrument
                        //$Instrument->save();
                    }

                    $Instrument = NDB_BVL_Instrument::factory($TableName, $row['CommentId2'], '', false);
                    if($Instrument != null) {
                        $Instrument->score();
                        //$Instrument->save();
                    }
                }
            }
        }

        $this->tpl_data['success'] = true;

        // freeze it, just in case
        //$this->form->freeze();
    }

    function conflicts_resolve()
    {
        $DB =& Database::singleton();
        $config =& NDB_Config::singleton();
        $cr_config = $config->getSetting("ConflictResolver");
        if (isset($cr_config['MaxConflicts'])) {
            $MaxConflicts = $cr_config['MaxConflicts'];
        } else {
            $MaxConflicts = 500;
        }

 		//Extract the fields from the parameter_type table
        $conflicts = array();


        $instruments_q = $DB->pselect("SELECT Test_name FROM test_names", array());

        // This array_map call turns the result of the above query to
        // a non-associative array, so that we can use the array_combine
        // trick to get an array of the form array( 'test_name' => 'test_name')
        // which can be used by quickforms for the dropdown. The array_merge
        // call after that just adds an Any/All Instruments option.

        
        $instruments = Utility::getAllInstruments();
        $instruments = array_merge(array('' => 'All Instruments'), $instruments);
        
        $sites = array_flip(array_merge(array('All'=>''), array_flip(Utility::getSiteList())));
        
        $visits = Utility::getVisitList();
        $visits = array_merge(array('' => 'All'), $visits);

        
        $extra_where = '';
        $user = User::singleton();
        if(!$user->hasPermission('access_all_profiles')) {
            // Only show current site in the filter
            $sites = array('' => '', $user->getCenterID() => $user->getSiteName());
            // And update the query to only select the current site
            $extra_where .= "AND s.CenterID = " . $DB->quote($user->getCenterID());
        }
        // Filter selection elements
        $this->addSelect('site', 'Site:', $sites);
        $this->addSelect('Instrument', 'Instrument:', $instruments);
        $this->addSelect('visit', 'Visit label:', $visits);
        $this->addBasicText('Question', 'Question:');
        $this->addBasicText('CandID', 'DCCID:');
        $this->addBasicText('PSCID', 'PSCID:');

        //This condition is commmented out because the $_REQUEST['site'] will always be empty (All) by default (which will show all the sites)
        //if(!empty($_REQUEST['site'])) {
            function AddWhere($Column, $Filter) {
                if(!empty($_REQUEST[$Filter]) && $_REQUEST[$Filter] != 'all') {
                    global $DB;
                    return " AND $Column LIKE " . $DB->quote($_REQUEST[$Filter] . '%');
                }
                return '';
            }

            $extra_where .= AddWhere("TableName", 'Instrument');
            $extra_where .= AddWhere("FieldName", 'Question');
            $extra_where .= AddWhere("s.CandID", 'CandID');
            $extra_where .= AddWhere("ca.PSCID", 'PSCID');
            $extra_where .= AddWhere("s.CenterID", 'site');
            $extra_where .= AddWhere("s.Visit_label", 'visit');


            // The 1=1 condition is just so that we can AND all the conditions together without worrying about whether or not
            // it's the first condition to be added.
            $DB->select("SELECT TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2, Value1, Value2, Visit_label, MD5(CONCAT_WS(':',TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2)) AS hash FROM conflicts_unresolved c LEFT JOIN flag f ON (c.CommentId1=f.CommentID) LEFT JOIN session s ON (f.SessionID=s.ID) LEFT JOIN candidate ca ON (ca.CandID=s.CandID) WHERE 1=1 $extra_where LIMIT $MaxConflicts", $conflicts);
            //If there are no conflicts print a message to that effect
            if(empty($conflicts)){
                $this->addScoreColumn("status", "No conflicts matching criteria found.");
                return;
            } else {
                $DB->selectRow("SELECT COUNT(*) as total FROM conflicts_unresolved c LEFT JOIN flag f ON (c.CommentId1=f.CommentID) LEFT JOIN session s ON (f.SessionID=s.ID) LEFT JOIN candidate ca ON (ca.CandID=s.CandID) WHERE 1=1 $extra_where", $conflicts_num);
                $this->addScoreColumn('total', 'Showing ' . count($conflicts) . ' of ' . $conflicts_num['total'] . ' conflicts matching filter.');
            }

	// Show conflicts resolved
	$resolved_conflicts = $DB->pselect("SELECT TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2, Visit_label, OldValue1, OldValue2, NewValue, MD5(CONCAT_WS(':',TableName, ExtraKeyColumn, ExtraKey1, ExtraKey2, FieldName, CommentId1, CommentId2, OldValue1, OldValue2, NewValue)) AS hash FROM conflicts_resolved c LEFT JOIN flag f ON (c.CommentId1=f.CommentID) LEFT JOIN session s ON (f.SessionID=s.ID) LEFT JOIN candidate ca ON (ca.CandID=s.CandID) WHERE 1=1 $extra_where LIMIT $MaxConflicts", array());
	if (empty($resolved_conflicts)) {
		$this->addScoreColumn("resolved_status", "No resolved conflicts matching criteria found.");
		return;
	} else {
		$resolved_conflicts_num = $DB->pselectRow("SELECT COUNT(*) as resolved_total FROM conflicts_resolved c LEFT JOIN flag f ON (c.CommentId1=f.CommentID) LEFT JOIN session s ON (f.SessionID=s.ID) LEFT JOIN candidate ca ON (ca.CandID=s.CandID) WHERE 1=1 $extra_where", array());
		$this->addScoreColumn('resolved_total', 'Showing ' . count($resolved_conflicts) . ' of ' . $resolved_conflicts_num['resolved_total'] . ' resolved conflicts matching filter.');
	}

	foreach($resolved_conflicts AS $resolved_conflict) {
            $hash = 'conflict_' . $resolved_conflict['hash'];
            $dccid = substr($resolved_conflict['CommentId1'],0,6);
            $ssid = substr($resolved_conflict['CommentId1'],6,7);
            if ($resolved_conflict['NewValue'] == 1) {
		$NewValue = $resolved_conflict['OldValue1'];
            } elseif ($resolved_conflict['NewValue'] == 2) {
                $NewValue = $resolved_conflict['OldValue2'];
            }
            $resolved_conflict_array = array(
                'name' => 'conflict_' . $resolved_conflict['hash'],
                'instrument' => $resolved_conflict['TableName'],
                'dccid' => $dccid,
                'pscid' => $ssid,
                'field' => $resolved_conflict['FieldName'],
                'visit_label' => $resolved_conflict['Visit_label'],
                'new_value' => $NewValue
            );
            $this->tpl_data['resolved_elements_list_names'][]= $hash;
            $this->tpl_data['resolved_elements_array'][$hash] = $resolved_conflict_array;
	}

        //Loop through the conflicts and add a row per
        foreach($conflicts AS $conflict) {
	    $indent = "&nbsp;&nbsp;&nbsp;&nbsp;";
            $hash = 'conflict_' . $conflict['hash'];
            $dccid = substr($conflict['CommentId1'],0,6);
            $ssid = substr($conflict['CommentId1'],6,7);
            if($conflict['FieldName'] == 'Examiner') {
                $name1 = $conflict['Value1'];
                $name2 = $conflict['Value2'];
                if($conflict['Value1'] != '') {
                    $name1 = $DB->pselectOne("SELECT Full_Name FROM examiners WHERE examinerID=:EID",
                        array('EID' =>$conflict['Value1'])
                    );
            	    if(Utility::isErrorX($name1)) {
		    	        $name1 = $conflict['Value1'];
		            }
                }

                if($conflict['Value2'] != '') {
                    $name2 = $DB->pselectOne("SELECT Full_Name FROM examiners WHERE examinerID=:EID",
                        array('EID' => $conflict['Value2'])
                    );
           	        if(Utility::isErrorX($name2)) {
			            $name2 = $conflict['Value2'];
		            }
                }
                $options = array('none'=>'Unresolved', '1'=> $name1, '2'=> $name2);
            } else {
                $options = array('none'=>'Unresolved', '1'=>$conflict['Value1'], '2'=>$conflict['Value2']);
            }
            $conflict_array = array(
                'name' => 'conflict_' . $conflict['hash'],
                'instrument' => $conflict['TableName'],
                'dccid' => $dccid,
                'pscid' => $ssid,
                'field' => $conflict['FieldName'],
                'visit_label' => $conflict['Visit_label']

            );
            $this->addSelect($hash, "$conflict[TableName] $indent $dccid $indent $ssid  $indent $conflict[FieldName]", $options);
            $this->tpl_data['elements_list_names'][]= $hash;
            $this->tpl_data['elements_array'][$hash] = $conflict_array;
        }

    }
}
?>

<?php
/**
 * This file handles the Document Repository for LORIS
 *
 * PHP Version 5
 *
 * @category Documentation
 * @package  Main
 * @author   Justin Kat <justinkat@gmail.com>, Mia Petkova <mia.petkova@gmail.com>
 * @license  Loris license
 * @link     https://www.github.com/Jkat/Loris-Trunk/
 */

require_once 'NDB_Menu_Filter.class.inc';

/**
 * Document Repository Class
 *
 * This class is for the Document Repository
 * 
 * @category Documentation
 * @package  Main 
 * @author   Justin Kat <justinkat@gmail.com>, Mia Petkova <mia.petkova@gmail.com>
 * @license  Loris license
 * @link     https://www.github.com/Jkat/Loris-Trunk/
*/

class NDB_Menu_Filter_Document_Repository extends NDB_Menu_Filter
{

    var $centerIDMap;
    var $treeToArray = array();
    
    function _hasAccess()
    {
        
        //create user object
        $user =& User::singleton();
        if (Utility::isErrorX($user)) {
            return PEAR::raiseError("User error: " . $user->getMessage());
        }

        return $user->hasPermission('file_upload');
    
    }

    function _setupVariables()
    {
        $user =& User::singleton();
        if (Utility::isErrorX($user)) {
            return PEAR::raiseError("User Error: ".$user->getMessage());
        }

        // create the centerID map
        $db =& Database::singleton();
        $pscRows = $db->pselect("SELECT CenterID, Name FROM psc", array());
        foreach ($pscRows AS $row) {
            $this->centerIDMap[$row['CenterID']] = $row['Name'];
        }
        
        $query = " FROM document_repository v"; 
        $query .= " WHERE (v.hide_video=false OR v.hide_video IS NULL)";

        // set the class variables
        $this->columns = array('v.record_id', 'v.File_name', 'v.version', 'v.File_size', 'v.File_category',
                               'v.File_type', 'v.Instrument', 'v.uploaded_by', 'v.For_site', 'v.comments',
                               'v.Data_dir', 'v.Date_uploaded');

        $openAccordion = $_GET['openAccordion'];	
        $this->tpl_data['openAccordion'] = $openAccordion;
        $filtered = $_GET['filtered'];	
        $this->tpl_data['filtered'] = $filtered;
        $this->query = $query;
        $this->group_by = '';
        $this->order_by = 'v.File_name';
        $this->headers = array('File_name', 'version', 'File_type', 'Instrument', 'uploaded_by', 'For_site', 'comments', 'Date_uploaded', 'Edit', 'Delete');
        $this->validFilters = array('v.For_site', 'v.File_name', 'v.File_category', 'v.File_type', 'v.version', 'v.uploaded_by');

        $this->formToFilter = array(
                                    'File_name' => 'v.File_name',
                                    'File_type' => 'v.File_type',
                                    'File_category' => 'v.File_category',
                                    'version' => 'v.version',
                                    'Data_dir'  => 'v.Data_dir',
                                    'For_site' => 'v.For_site',
                                    'uploaded_by' => 'v.uploaded_by',
                                    'Date_uploaded' => 'v.Date_uploaded'
                                    );
        return true;
    }


    function _setFilterForm()
    {
        // create user object
        $user =& User::singleton();
        if (Utility::isErrorX($user)) {
            return PEAR::raiseError("User Error: ".$user->getMessage());
        }

        $list_of_sites = Utility::getSiteList(true, false);
           // allow to view all sites data through filter
        if ($user->hasPermission('access_all_profiles')) { 
            // get the list of study sites - to be replaced by the Site object
            if (Utility::isErrorX($list_of_sites)) {
                return PEAR::raiseError("DB Error: ".$list_of_sites->getMessage());
            }
            if (is_array($list_of_sites)) $list_of_sites = array(null => 'Any') + $list_of_sites;
        } else {
            // allow only to view own site data
            $site =& Site::singleton($user->getData('CenterID'));
            if (Utility::isErrorX($site)) {
                return PEAR::raiseError("DB Error: ".$site->getMessage());
            }
            if ($site->isStudySite()) {
                $list_of_sites = array($user->getData('CenterID') => $user->getData('Site'));
            }
        }

        $list_of_instruments = Utility::getAllInstruments();
        
        $list_of_visit_labels = array_merge(array(null=>"Any") + Utility::getVisitList());
        $subproject_options = Utility::getSubprojectList();
        $subproject_options = array(null=>'Any') + $subproject_options;

        $DB = Database::singleton();
        
        $fileNames = $DB->pselect("Select File_name from document_repository", array());
        $fileTypes = array();
        $fileT = array();
        foreach ($fileNames as $name) {
            $fileExt[$name['File_name']] = $name['File_name'];
        }

        if (is_array($fileExt)) {
            foreach ($fileExt as $field=>$value) {
                    $parts = pathinfo($value);
                    $extension = $parts['extension'];
                    $DB->update("document_repository", array("File_type"=>$extension), array('File_name'=>$value));
                    $fileT = $DB->pselect("SELECT DISTINCT File_type FROM document_repository order by File_type", array());
            }
        }

        foreach ($fileT as $fileType) {
            $fileTypes[$fileType['File_type']] = $fileType['File_type'];
        }

        $fileTypes = array_merge(array(null=>'Any') + $fileTypes);

        // Form Elements
        $this->addBasicText('File_name', 'File name:'); 
        $this->addBasicText('version', 'Version:'); 

        $results = array();
        $dbresult= $DB->pselect("SELECT * FROM document_repository_categories order by parent_id asc", array());
        foreach ($dbresult as $row) {
            $results[]=$row;
            $tree = null;
            foreach ($results as $result) {
                $thisref = &$refs->{$result['id']};
                foreach ($result as $k => $v) {
                        $thisref->{$k} = $v;
                }
                if ($result['parent_id'] == 0) {
                    $tree->{$result['id']} = &$thisref;
                } else {
                    $refs->{$result['parent_id']}->children->{$result['id']} = &$thisref;
                }
            }
            $tree; // contains the newly sorted tree.
        }

        $IDCategories = $DB->pselect("SELECT id,category_name,parent_id,comments FROM document_repository_categories order by parent_id", array());	
        $tmpIDCategories = $IDCategories;

        //$tree is an object with references, so here we are encoding it as json and then decoding it into a php associative array
        $tmpIDCategories = json_decode(json_encode($tree), true);
        $categoryOrdering = $this->parseTree($tmpIDCategories); 

        foreach ($categoryOrdering as $order) {
            foreach ($IDCategories as $key=>$value) {
                if ($value['id'] == $order) {
                    $fileCategories[$value['id']] = $this->parseCategory($value);
                    $fileComments[$value['id']] = $value['comments'];
                }
            }
        }

        $this->addSelect('File_type', 'File type:', $fileTypes); 
        $this->addSelect('File_category', 'File category:', $fileCategories); 
        $this->tpl_data['File_categories'] = $fileCategories;
        $this->tpl_data['File_comments'] = $fileComments;
        $this->tpl_data['Sites'] = $list_of_sites;
        $this->tpl_data['Instruments'] = $list_of_instruments;
        $user =& User::singleton();
        $userID = $user->getData('UserID');
        $this->tpl_data['User'] = $userID;
        $this->addSelect('For_site', 'For Site:', $list_of_sites);
        $this->addBasicText('uploaded_by', 'Uploaded By:');
       
        return true;
    }

    function getBetween($string,$delim)
    {
        $string = explode($delim, $string);
        return isset($string[0]) ? $string[0] : '';
    }

    function walkTree($item,$key)
    {
        if ($key == 'id') {
            $this->treeToArray[]= $item;
        }
    }

    function parseTree($treeToParse)
    {
        foreach ($treeToParse as $theTree) {
            array_walk_recursive($theTree, array($this,'walkTree'));
        }
        return $this->treeToArray;
    }

    function parseCategory($value)
    {
        $depth = 0;
        $DB = Database::singleton();
            $categoryName = $value['category_name'];
        do {
            if ($value['parent_id'] != 0) {
                $depth += 1;
                $parent_id = $value['parent_id'];
                $value = $DB->pselectRow("SELECT * FROM document_repository_categories where id=$parent_id", array());
                $categoryName = $value['category_name'] . ">" . $categoryName;
            }
        } while ($value['parent_id'] != 0);
        return $categoryName;
    }

    /**
     * Sets the template data for the list of candidates, users, etc.
     *
     * @return void
     * @throws PEAR_Error
     * @access private
     */
    function _setDataTable()
    {
        // create instance of config object
        $config =& NDB_Config::singleton();

        // overriding pagination
        $DB = Database::singleton();
        $perpage = $DB->pselectOne("SELECT count(*) FROM document_repository", array());
        // configure the page pagination
        $params = array(
            'mode'       => 'Sliding',
            'perPage'    => $perpage,
            'delta'      => 3,
            'itemData'   => $this->list
        );
        $pager = Pager::factory($params);
        $this->list = $pager->getPageData();
        $links = $pager->getLinks();
        $this->tpl_data['page_links'] = $links['all'];

        // print out column headings
        $i = 0;
        foreach ($this->headers as $header) {
            $this->tpl_data['headers'][$i]['name'] = $header;
            // format header
            $this->tpl_data['headers'][$i]['displayName'] = ucwords(str_replace('_', ' ', $header));
            // set field ordering
            if (isset($this->filter['order'])) {
                $this->tpl_data['headers'][$i]['fieldOrder'] = ($this->filter['order']['fieldOrder'] == 'ASC') ? 'DESC' : 'ASC';
            }
            $i++;
        }

        // get the template data for the table rows
        if (is_array($this->list) && !empty($this->list)) {
            // print out data rows
            $count = $pager->getOffsetByPageId();
            $success = $this->_setDataTableRows($count[0]);
            if (Utility::isErrorX($success)) {
                return PEAR::raiseError("NDB_Filter::_setDataTable(): ".$success->getMessage());
            }
        }
    }



    function _setDataTableRows($count)
    {
        // This is a hack. It shouldn't be required, and it certainly shouldn't be here.
        $user =& User::singleton();
        $this->tpl_data['Normal_Perm'] = ($user->hasPermission('file_upload') || $user->hasPermission('data_entry'));
        // end hack

        $site_names = Utility::getSiteList(true, false); 
        // print out
        $x = 0;
        foreach ($this->list as $item) {
            //count column
            $this->tpl_data['items'][$x][0]['value'] = $x + $count;

            //print out data rows
            $i = 1;
            $Data_dir = $item['Data_dir'];
            $record_id = $item['record_id'];
            $File_size = $item['File_size'];
            $File_category = $item['File_category'];
            foreach ($item as $key => $val) {

                // Confused about this line
                if ($key == 'Data_dir') continue;
                if ($key == 'record_id') continue;
                if ($key == 'File_size') continue;

                $this->tpl_data['items'][$x][$i]['name'] = $key;


                if ($key == 'For_site') {
                    $this->tpl_data['items'][$x][$i]['value'] = $site_names[$val];
                } else {
                    $this->tpl_data['items'][$x][$i]['value'] = $val;
                }

                if ($key == 'File_name') {
                    $this->tpl_data['items'][$x][$i]['Data_dir'] = $Data_dir;
                    $this->tpl_data['items'][$x][$i]['File_size'] = $this->format_size($File_size);
                }
                
                if ($key == 'Date_uploaded') {
                    $this->tpl_data['items'][$x][$i+1]['name'] = 'record_id';
                    $this->tpl_data['items'][$x][$i+1]['value'] = $record_id;
                    $this->tpl_data['items'][$x][$i+2]['name'] = 'record_id_delete';
                    $this->tpl_data['items'][$x][$i+2]['value'] = $record_id;
                }

                if ($key == 'File_category') {
                    $this->tpl_data['items'][$x][$i]['File_category'] = $File_category;
                }                


                $i++;
            }
            $x++;
        }
        return true;
    }
    
    function format_size($value)
    {
        if (is_null($value)) {
            return null;
        }
      
        if ($value >= 1000000000) {
            $value = floor($value/1000000000);
            return "{$value} GB";
        }
        if ($value >= 1000000) {
            $value = floor($value/1000000);
            return "{$value} MB";
        }
        if ($value >= 1000) {
            $value = floor($value/1000);
            return "{$value} KB";
        }
      
        return "{$value} bytes";
      
    }
}

?>

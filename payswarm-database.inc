<?php
$payswarm_db_version = "1.0";

function payswarm_install_database()
{
   global $wpdb;
   global $payswarm_db_version;

   $table_name = $wpdb->prefix . "payswarm_tokens";
   if($wpdb->get_var("SHOW TABLES LIKE '$table_name'") != $table_name) 
   {
      $sql = 'CREATE TABLE ' . $table_name . 
         '(id VARCHAR(255) NOT NULL, state TINYTEXT, token TINYTEXT, ' .
         'secret TINYTEXT, amount TINYTEXT, ' .
         'mtime TIMESTAMP DEFAULT CURRENT_TIMESTAMP, ' .
         'PRIMARY KEY (id))';

      require_once(ABSPATH . 'wp-admin/includes/upgrade.php');
      dbDelta($sql);

      add_option("payswarm_db_version", $payswarm_db_version);
   }
}
?>

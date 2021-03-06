<?php

/**
 * List all zimbra accounts page callback.
 */
function zmt_account_list_all() {
  global $user;

  drupal_set_title(t('Accounts'));
  $header = array();
  $header[] = array('data' => t('No.'));
  $header[] = array('data' => t('Name'), 'field' => 'za.name');
  $header[] = array('data' => t('Full name'), 'field' => 'za.full_name');
  $header[] = array('data' => t('COS'), 'field' => 'zc.name');
  $header[] = array('data' => t('Status'), 'field' => 'za.status');
  $header[] = array('data' => t('Operations'));

  $query = db_select('zmt_account', 'za')->extend('PagerDefault')->extend('TableSort');
  $query->join('zmt_domain', 'zd', 'zd.id = za.domain_id');
  $query->leftJoin('zmt_cos', 'zc', 'zc.id = za.cos_id');
  $query->addField('zd', 'name', 'domain');
  $query->addField('zc', 'name', 'cos');

  $domain_ids = array();
  if (!(user_access('administer zmt'))) {
    if (user_access('administer any zmt_account')) {
      $domains = zmt_domain_load_multiple_by_user($user);
      $domain_ids = array_keys($domains);
      if (empty($domain_ids)) {
        $domain_ids = array(0);
      }

      $domain_id = isset($_SESSION['zmt_filter_domain_id']) ? (int) $_SESSION['zmt_filter_domain_id'] : 0;
      if ($domain_id) {
        $query->condition('za.domain_id', $domain_id);
      }
      else {
        $query->condition('za.domain_id', $domain_ids);
        $query->addTag('account_query_filter');
      }
    }
    else {
      $query->condition('za.uid', $user->uid);
    }
  }
  else {
    $domain_id = isset($_SESSION['zmt_filter_domain_id']) ? (int) $_SESSION['zmt_filter_domain_id'] : 0;
    if ($domain_id) {
      $query->condition('za.domain_id', $domain_id);
    }
  }

  $item_per_page = variable_get('zimbra_item_per_page', 20);
  $result = $query
    ->fields('za')
    ->orderByHeader($header)
    ->limit($item_per_page)
    ->execute();

  $destination = drupal_get_destination();
  $statuses = zmt_account_statuses();
  $rows = array();
  foreach ($result as $key => $account) {
    $row = array();
    $row[] = $key + 1 + (pager_find_page() * $item_per_page);
    $account_name = $account->name . '@' . $account->domain;
    $row[] = l($account_name, 'zmt/account/' . $account->id . '/edit', array('query' => $destination));
    $row[] = array(
      'data' => $account->full_name,
    );
    $row[] = array(
      'data' => $account->cos,
    );
    $row[] = array(
      'data' => isset($statuses[$account->status]) ? $statuses[$account->status] : $account->status,
    );

    $operations = l(t('edit'), 'zmt/account/' . $account->id . '/edit', array('query' => $destination));
    $operations .= '&nbsp;&nbsp;&nbsp;' . l(t('delete'), 'zmt/account/' . $account->id . '/delete', array('query' => $destination));
    $row[] = $operations;
    $rows[] = $row;
  }
  if (empty($rows)) {
    $rows[] = array(
      array(
        'data' => t('There are no accounts available.') . ' ' . l(t('Add one now'), 'zmt/account/add') . '.',
        'colspan' => count($header)
      ),
    );
  }

  $build = array();
  if (user_access('administer zmt')) {
    $build['account_filter'] = drupal_get_form('zmt_filter_by_domain_form');
  }
  elseif (user_access('administer any zmt_account')) {
    if (count($domain_ids) > 1) {
      $build['account_filter'] = drupal_get_form('zmt_filter_by_domain_form');
    }
  }
  $build['account_table']  = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );
  $build['account_pager'] = array('#theme' => 'pager');

  return $build;
}

/**
 * Add new zimbra account page callback.
 */
function zmt_account_add_page() {
  drupal_set_title(t('Add Account'));
  $account = zmt_account_new();
  return drupal_get_form('zmt_account_edit_form', $account);
}

/**
 * Edit exists zimbra account page callback.
 */
function zmt_account_edit_page($account) {
  drupal_set_title(t('Edit Account'));
  return drupal_get_form('zmt_account_edit_form', $account);
}

/**
 * Zimbra account edit form.
 */
function zmt_account_edit_form($form, &$form_state, $account){
  global $user;

  $form_state['account'] = $account;

  $form['name'] = array(
    '#title' => t('Name'),
    '#type' => 'textfield',
    '#default_value' => $account->name,
    '#required' => TRUE,
  );

  $domain_id = isset($form_state['values']['domain_id']) ? (int) $form_state['values']['domain_id'] : (int) $account->domain_id;
  $options = array();
  if (!empty($account->id)) {
    $domain = zmt_domain_load($account->domain_id);
    if ($domain) {
      $domain_id = $domain->id;
      $options[$domain->id] = $domain->name;
    }
  }
  else {
    if (!(user_access('administer zmt'))) {
      $domains = zmt_domain_load_multiple_by_user($user);
    }
    else {
      $domains = zmt_domain_load_all();
    }
    if (count($domains) !== 1) {
      $options = array('' => t('- None -'));
    }
    elseif (count($domains) == 1) {
      $domain = reset($domains);
      $domain_id = $domain->id;
    }
    foreach ($domains as $domain) {
      $options[$domain->id] = $domain->name;
    }
  }
  $form['domain_id'] = array(
    '#type' => 'select',
    '#title' => t('Domain'),
    '#options' => $options,
    '#default_value' => $account->domain_id,
    '#required' => TRUE,
    '#ajax' => array(
      'callback' => 'zmt_account_ajax_domain_select_callback',
      'wrapper' => 'zmt-cos-wrap',
    ),
  );

  $options = array();
  if ($domain_id) {
    $mappings = zmt_domain_cos_load_multiple_by_domain(array($domain_id));
    foreach ($mappings as $map) {
      $options[$map->cos_id] = $map->cos;
    }
  }
  $form['cos_id'] = array(
    '#type' => 'select',
    '#title' => t('Class of Service'),
    '#options' => $options,
    '#empty_option' => t('- None -'),
    '#default_value' => $account->cos_id,
    '#required' => TRUE,
    '#prefix' => '<div id="zmt-cos-wrap">',
    '#suffix' => '</div>',
  );

  $form['password'] = array(
    '#type' => 'password_confirm',
    '#required' => !(isset($account->id) && (int) $account->id > 0),
    '#size' => 45,
  );
  $form['full_name'] = array(
    '#title' => t('Full name'),
    '#type' => 'textfield',
    '#default_value' => $account->full_name,
    '#required' => TRUE,
  );
  $options = zmt_account_statuses();
  $form['status'] = array(
    '#type' => 'select',
    '#title' => t('Status'),
    '#options' => $options,
    '#default_value' => $account->status,
  );
  $form['title'] = array(
    '#title' => t('Work title'),
    '#type' => 'textfield',
    '#default_value' => $account->title,
  );
  $form['telephone'] = array(
    '#title' => t('Telephone'),
    '#type' => 'textfield',
    '#default_value' => $account->telephone,
  );
  $form['mobile'] = array(
    '#title' => t('Mobile'),
    '#type' => 'textfield',
    '#default_value' => $account->mobile,
  );
  $form['company'] = array(
    '#title' => t('Company'),
    '#type' => 'textfield',
    '#default_value' => $account->company,
  );

  $form['hidden_forwarding'] = array(
    '#title' => t('Hidden forwarding'),
    '#type' => 'fieldset',
  );

  $addresses = drupal_json_decode($account->hidden_forwarding);
  if (!empty($addresses)) {
    foreach ($addresses as $key => $address) {
      $form['hidden_forwarding']['address_' . $key] = array(
        '#type' => 'textfield',
        '#name' => 'address[]',
        '#size' => 45,
        '#default_value' => $address,
        '#theme_wrappers' => array(),
        '#prefix' => '<div class="form-item form-type-textfield">',
        '#suffix' => ' <a href="#" class="zmt-remove-parent">' . t('Remove') . '</a></div>',
      );
    }
  }
  else {
    $form['hidden_forwarding']['address_0'] = array(
      '#type' => 'textfield',
      '#name' => 'address[]',
      '#size' => 45,
      '#theme_wrappers' => array(),
      '#prefix' => '<div class="form-item form-type-textfield">',
      '#suffix' => ' <a href="#" class="zmt-remove-parent">' . t('Remove') . '</a></div>',
    );
  }

  $form['hidden_forwarding']['add'] = array(
    '#type' => 'button',
    '#value' => t('Add address'),
    '#limit_validation_errors' => array(),
    '#ajax' => array(
      'callback' => 'zmt_account_edit_ajax_add_address',
      'event' => 'click',
      'prevent' => 'submit click mousedown',
    ),
  );

  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#submit' => array('zmt_account_edit_form_submit'),
    '#suffix' => l(t('Cancel'), 'zmt/account'),
  );

  $form['#validate'][] = 'zmt_account_edit_form_validate';
  $path = drupal_get_path('module', 'zmt_core');
  $form['#attached']['js'] = array(
    $path . '/js/zmt.js',
  );

  return $form;
}

/**
 * Zimbra account edit form validate callback.
 */
function zmt_account_edit_form_validate($form, &$form_state) {
  $values = $form_state['values'];
  $account = $form_state['account'];

  $domain_id = (int) $values['domain_id'];
  $cos_id = (int) $values['cos_id'];
  $name = trim($values['name']);
  $id = isset($account->id) ? (int) $account->id : 0;

  $exists = zmt_account_exists($domain_id, $name, $id);
  if ($exists === TRUE) {
    form_set_error('name', t('The mailbox name already exists in selected domain.'));
  }

  if ($id === 0) {
    $domain_cos = zmt_domain_cos_load_single($domain_id, $cos_id);
    $max_account = !empty($domain_cos->max_account) ? (int) $domain_cos->max_account : 0;
    $total_account = zmt_account_count_by_domain($domain_id) + 1;
    if ($max_account && $total_account > $max_account) {
      form_set_error('name', t('The domain has reached the maximum number of accounts.'));
    }
  }

  $hidden_forwarding = array();
  $addresses = !empty($_POST['address']) ? $_POST['address'] : array();
  $addresses = array_map('trim', $addresses);
  foreach ($addresses as $address) {
    $address = check_plain($address);
    if (!empty($address) && valid_email_address($address)) {
      $hidden_forwarding[] = $address;
    }
  }
  $form_state['hidden_forwarding'] = $hidden_forwarding;
}

/**
 * Zimbra account ajax domain select callback.
 */
function zmt_account_ajax_domain_select_callback($form, $form_state) {
  return $form['cos_id'];
}

/**
 * Zimbra account ajax add address callback.
 */
function zmt_account_edit_ajax_add_address($form, $form_state) {
  $commands = array();
  $address = array(
    '#type' => 'textfield',
    '#name' => 'address[]',
    '#size' => 45,
    '#theme_wrappers' => array(),
    '#prefix' => '<div class="form-item form-type-textfield">',
    '#suffix' => ' <a href="#" class="zmt-remove-parent">' . t('Remove') . '</a></div>',
  );
  $selector = '#edit-add';
  $commands[] = ajax_command_before($selector, drupal_render($address));
  return array('#type' => 'ajax', '#commands' => $commands);
}

/**
 * Zimbra account edit form submit callback.
 */
function zmt_account_edit_form_submit($form, &$form_state) {
  $account = $form_state['account'];
  $values = &$form_state['values'];

  $account->domain_id = (int) $values['domain_id'];
  $account->cos_id = (int) $values['cos_id'];
  $account->name = filter_var($values['name'], FILTER_SANITIZE_EMAIL);
  $account->password = check_plain($values['password']);
  $account->full_name = check_plain($values['full_name']);
  $account->status = check_plain($values['status']);
  $account->title = check_plain($values['title']);
  $account->telephone = check_plain($values['telephone']);
  $account->mobile = check_plain($values['mobile']);
  $account->company = check_plain($values['company']);
  $account->hidden_forwarding = drupal_json_encode($form_state['hidden_forwarding']);

  zmt_account_save($account);

  $form_state['redirect'] = 'zmt/account';
}


/**
 * Import accounts page callback.
 */
function zmt_account_import_page() {
  drupal_set_title(t('Import Accounts'));
  return drupal_get_form('zmt_account_import_form');
}

/**
 * Import accounts form
 */
function zmt_account_import_form($form, &$form_state){
  if (empty($form_state['time'])) {
    $form_state['time'] = REQUEST_TIME;
  }

  $form['#prefix'] = '<div id="zmt-import-account-wrapper">';
  $form['#suffix'] = '</div>';

  $form['file'] = array(
    '#type' => 'file',
    '#title' => t('Csv file'),
    '#description' => t('Csv file fields: name,password,full_name,status,title,telephone,mobile,company'),
  );

  $form['domain_id'] = array(
    '#type' => 'zmt_domain_select',
    '#required' => TRUE,
    '#ajax' => array(
      'callback' => 'zmt_account_ajax_domain_select_callback',
      'wrapper' => 'zmt-cos-wrap',
    ),
  );

  $domain_id = isset($form_state['values']['domain_id']) ? (int) $form_state['values']['domain_id'] : 0;
  $options = array();
  if ($domain_id) {
    $mappings = zmt_domain_cos_load_multiple_by_domain(array($domain_id));
    foreach ($mappings as $map) {
      $options[$map->cos_id] = $map->cos;
    }
  }
  $form['cos_id'] = array(
    '#type' => 'select',
    '#title' => t('Class of Service'),
    '#options' => $options,
    '#empty_option' => t('- None -'),
    '#required' => TRUE,
    '#prefix' => '<div id="zmt-cos-wrap">',
    '#suffix' => '</div>',
  );

  $form['actions']['import'] = array(
    '#type' => 'submit',
    '#value' => t('Import'),
    '#ajax' => array(
      'callback' => 'zmt_account_ajax_import_form_callback',
      'wrapper' => 'zmt-import-account-wrapper',
      'progress' => array(
        'type' => 'bar',
        'message' => t('Importing data..'),
        'url' => url('zmt/account/importing/progress/' . $form_state['time']),
        'interval' => 1000,
      ),
    ),
    '#prefix' => '<div class="form-item ajax-actions">',
    '#suffix' => l(t('Cancel'), 'zmt/account') . '</div>',
  );

  $form['#attached']['css'] = array(
    drupal_get_path('module', 'zmt_core') .'/zmt.css',
  );

  $form['#validate'] = array('zmt_account_import_form_validate');

  return $form;
}

/**
 * Get the progress bar importing status, as JSON.
 *
 * This is the menu handler for
 * zmt/account/importing/progress/$time.
 *
 * @param int $time
 *   Timestamp.
 *
 * @see zmt_account_ajax_import_form_callback()
 */
function zmt_account_importing_progress($time) {
  $progress = array(
    'message' => t('Starting import...'),
    'percentage' => -1,
  );

  $message = variable_get('zmt_importing_message_' . $time, t('Importing data...'));
  $percentage = variable_get('zmt_importing_progress_' . $time, 0);

  if ($percentage) {
    $progress['message'] = $message;
    $progress['percentage'] = $percentage;
  }

  drupal_json_output($progress);
}

/**
 * Import accounts form validate callback
 */
function zmt_account_import_form_validate($form, &$form_state) {
  $op = isset($form_state['values']['op']);
  $domain_id = (int) $form_state['values']['domain_id'];
  $cos_id = (int) $form_state['values']['cos_id'];
  $accounts = array();

  if (!form_set_error() && ($op || !zmt_is_ajax_request())) {
    $destination = 'public://zmt/';
    if (file_prepare_directory($destination, FILE_CREATE_DIRECTORY)) {
      $file = file_save_upload(
        'file',
        array('file_validate_extensions' => array('csv')),
        $destination
      );
      if ($file) {
        $statuses = zmt_account_statuses();

        $handle = fopen($file->uri, 'r');
        $row = fgetcsv($handle);
        $columns = array();
        foreach ($row as $i => $header) {
          $columns[$i] = trim($header);
        }
        $col_number = count($columns);
        $count = 0;
        while ($row = fgetcsv($handle)) {
          $count++;
          if (count($row) == $col_number) {
            $record = array();
            foreach ($row as $i => $field) {
              if ($columns[$i] === 'status' && !in_array($field, $statuses)) {
                $field = 'active';
              }

              $column = $columns[$i];
              if ($column) {
                $record[$column] = $field;
              }
            }
            $account = zmt_account_prepare($record, $domain_id);
            if (!empty($account->name) && !empty($account->password)) {
              $account->cos_id = $cos_id;
              $exists = zmt_account_exists($domain_id, $account->name);
              if (!$exists) {
                $accounts[] = $account;
              }
            }
            else {
              drupal_set_message(t('Row @count is empty name or password', array(
                '@count' => $count,
              )), 'error');
            }
          }
          else {
            drupal_set_message(t('Row @count is invalid', array(
              '@count' => $count,
            )), 'error');
          }
        }
        fclose($handle);
      }
    }
  }
  if (!empty($accounts)) {
    $domain_cos = zmt_domain_cos_load_single($domain_id, $cos_id);
    $max_account = !empty($domain_cos->max_account) ? (int) $domain_cos->max_account : 0;
    $total_account = zmt_account_count_by_domain($domain_id);
    $total_account += count($accounts);
    if ($max_account && $total_account > $max_account) {
      form_set_error('file', t('The domain has reached the maximum number of accounts.'));
    }
  }
  $form_state['accounts'] = $accounts;
}

/**
 * Import accounts form callback
 */
function zmt_account_ajax_import_form_callback($form, $form_state) {
  if (form_set_error()) {
    return $form;
  }
  else {
    $commands = array();
    $accounts = $form_state['accounts'];
    if (!empty($accounts)) {
      $count = 0;
      $total_account = count($accounts);
      $message_name = 'zmt_importing_message_' . $form_state['time'];
      $progress_name = 'zmt_importing_progress_' . $form_state['time'];
      variable_set($message_name, t('Importing accounts to Drupal...'));
      variable_set($progress_name, $count);

      foreach ($accounts as $account) {
        zmt_account_save($account);
        $count++;
        variable_set($progress_name, floor($count * 100 / $total_account));
      }
      drupal_set_message(t('@count accounts were updated.', array('@count' => $count)));
      sleep(1);
      variable_del($message_name);
      variable_del($progress_name);
    }
    $commands[] = zmt_ajax_command_redirect('zmt/account');
    return array(
      '#type' => 'ajax',
      '#commands' => $commands,
    );
  }
}

/**
 * Prepare account entity
 */
function zmt_account_prepare($record, $domain_id = 0) {
  $account = zmt_account_new();
  $account->domain_id = (int) $domain_id;
  foreach ($record as $field => $value) {
    $account->$field = trim($value);
  }
  if (isset($account->name)) {
    $account->name = filter_var($account->name, FILTER_SANITIZE_EMAIL);
  }
  return $account;
}

/**
 * Delete exists zimbra account page callback.
 */
function zmt_account_delete_page($account) {
  drupal_set_title(t('Delete Account'));
  return drupal_get_form('zmt_account_delete_form', $account);
}

/**
 * Form callback: confirmation form for deleting a account.
 *
 * @param $account
 *   The account object to be deleted.
 *
 * @see confirm_form()
 */
function zmt_account_delete_form($form, &$form_state, $account) {
  $form_state['account'] = $account;

  $form['#submit'][] = 'zmt_account_delete_form_submit';

  $description = '';
  if ((int) $account->delete_account > 0 && !empty($account->zimbra_id)) {
    $description .= t('Delete this account also delete it in zimbra server.');
  }
  $description .= t(' This action cannot be undone.');

  $form = confirm_form($form,
    t('Are you sure you want to delete account %name?', array('%name' => $account->name)),
    '',
    '<p>' . $description . '</p>',
    t('Delete'),
    t('Cancel'),
    'confirm'
  );

  return $form;
}

/**
 * Submit callback for zmt_account_delete_form().
 */
function zmt_account_delete_form_submit($form, &$form_state) {
  $account = $form_state['account'];
  zmt_account_delete($account->id);
  drupal_set_message(t('%name has been deleted.', array('%name' => $account->name)));
  watchdog('zmt_account', 'Deleted account %name.', array('%name' => $account->name), WATCHDOG_NOTICE);
}

/**
 * Mailbox Quota page 
 */ 
function zmt_account_quota_usage() {
  global $user;
  module_load_include('inc', 'zmt_core', 'includes/zmt.zimbra');
   
  drupal_set_title('Mailbox Quota');
  $header = array();
  $header[] = array('data' => t('No.'));
  $header[] = array('data' => t('Account name'));
  $header[] = array('data' => t('Used quota (MB)'));
  $header[] = array('data' => t('Quota limit (MB)'));

  $domain_id = isset($_SESSION['zmt_filter_domain_id']) ? (int) $_SESSION['zmt_filter_domain_id'] : 0;
  $domain = zmt_domain_load($domain_id);
  if ($domain) {
    $server_id = (int) $domain->server_id;
  }
  else {
    $server_id = isset($_SESSION['zmt_filter_server_id']) ? (int) $_SESSION['zmt_filter_server_id'] : 0;
  }

  $result = array();
  $rows = array();
  $item_per_page = variable_get('zimbra_item_per_page', 20);
  $total = 0;
  if ($server_id > 0) {
    $api = zimbra_admin_api_by_server($server_id);
    $pager = array();
    $limit = 0;
    $offset = 0;
    if ($domain) {
      $result = zimbra_get_quota_usage($api, $domain->name ,$limit, $offset, $pager);
      $total = $pager['total'];
      if($total > 0) {
        $page = isset($_GET['page']) ? $_GET['page'] : 0;
        $records_page = array_slice($result, $item_per_page * $page, $item_per_page);
        pager_default_initialize($total, $item_per_page, $element = 0);
        $mb = 1024 * 1024;   
        $key = 0;
        if($records_page) {
          foreach ($records_page as $account) {
            $row = array();
            $row[] = $key + 1 + $page * $item_per_page ;
            $row[] = array(
              'data' => $account->name,
            );
            $row[] = array(
              'data' => number_format($account->used / $mb, 2, '.', ' '),
            );
            $row[] = array(
              'data' => number_format($account->limit / $mb, 2, '.', ' '),
            );
            $rows[] = $row;
            $key ++ ;
          }
        }
      }
    }
  }
   
  if (empty($rows)) {
    $rows[] = array(
      array(
        'data' => t('There are no accounts available.') ,
        'colspan' => count($header)
      ),
    );
  }

  $build = array();
  $build['account_quota_filter'] = drupal_get_form('zmt_filter_by_domain_form');
  $build['account_quota_table']  = array(
    '#theme' => 'table',
    '#header' => $header,
    '#rows' => $rows,
  );
  $build['account_quota_pager'] = array('#theme' => 'pager','#quantity' => $total,);

  return $build;
}

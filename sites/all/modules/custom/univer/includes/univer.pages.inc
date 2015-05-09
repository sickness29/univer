<?php

/**
 * Form for create manual.
 */
function univer_manual_add_form($form, &$form_state) {
  $form['manual_title'] = array(
    '#title'    => t('Manual title'),
    '#type'     => 'textfield',
    '#required' => TRUE,
  );
  $form['manual_zip'] = array(
    '#type'           => 'file',
    '#title'          => t('Choose a file'),
    '#title_display'  => 'invisible',
    '#theme_wrappers' => array(),
  );
  $form['submit'] = array(
    '#type'  => 'submit',
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Form validate new manual.
 */
function univer_manual_add_form_validate($form, &$form_state) {
  $validators = array(
    'file_validate_extensions' => array('zip'),
  );
  if ($file = file_save_upload('manual_zip', $validators, 'public://zip/')) {
    $form_state['values']['manual_zip'] = $file;
  }
  else {
    form_set_error('file', 'Only zip');
  }
}

/**
 * Form submit new manual.
 */
function univer_manual_add_form_submit($form, &$form_state) {
  global $user;
  $manual_title = $form_state['values']['manual_title'];
  $manual_zip   = $form_state['values']['manual_zip'];

  // Fetch zip object.
  $zip = archiver_get_archiver($manual_zip->uri);

  // Create folder zip files.
  $mkdir_uri = file_build_uri("extract-{$manual_zip->fid}");
  drupal_mkdir($mkdir_uri, 0777, TRUE);
  $zip->extract($mkdir_uri);

  // Create folder pdf files.
  $pdf_uri = file_build_uri('pdf');
  if (!file_prepare_directory($pdf_uri)) {
    drupal_mkdir($pdf_uri, 0777, TRUE);
  }

  // Generate node.
  $node = new stdClass();
  $node->title = !empty($manual_title) ? $manual_title : '';
  $node->type = 'manual';
  $node->language = LANGUAGE_NONE;
  $node->uid = $user->uid; 
  $node->status = 1;
  node_save($node);

  // Add pdt files to node.
  foreach ($zip->listContents() as $zip_file) {
    $mimetype = file_get_mimetype($zip_file);

    // Save only pdf files.
    if ($mimetype != 'application/pdf') {
      continue;
    }

    // Generate file object for save.
    $file              = new stdClass();
    $file->uid         = $user->uid;
    $file->status      = 0;
    $file->filename    = trim(drupal_basename($zip_file), '.');
    $file->uri         = file_build_uri("extract-{$manual_zip->fid}/{$zip_file}");
    $file->filemime    = $mimetype;
    $file->display     = 1;
    $file->description = $file->filename;
    $file = file_move($file, file_build_uri("pdf/{$zip_file}"));
    file_save($file);

    // Generate collection object.
    $field_collection_value = entity_create('field_collection_item', array('field_name' => 'field_section'));
    $field_collection_value->setHostEntity('node', $node);
    $field_collection_value->field_file[LANGUAGE_NONE][0] = (array)$file;
    $field_collection_value->save(TRUE);
  }
  drupal_rmdir($mkdir_uri);
  field_attach_update('node', $node);

  $form_state['redirect'] = "node/{$node->nid}/edit";
}

/**
 * Pdf view.
 */
function univer_manual_section_callback($node, $section) {
  if (!$node->status && !user_access('pdf preview')) {
    drupal_access_denied();
  }

  // Fetch node sections.
  $section--;
  $sections = field_get_items('node', $node, 'field_section');

  // Check for isset necessary section.
  if (!isset($sections[$section])) {
    drupal_not_found();
  }
  $cid = $sections[$section]['value'];
  $field_collection = field_collection_item_load($cid);

  // Fetch file.
  $file = field_get_items('field_collection_item', $field_collection, 'field_file');
  $file = reset($file);

  // Reed file.
  $content = readfile($file['uri'],'r');

  // Output file.
  drupal_add_http_header('Content-Type', $file['filemime']);
  drupal_add_http_header('Content-Disposition', 'filename=' . $file['filename']);
  print($content);

  return '';
}

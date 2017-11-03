<?php
/**
 * @file
 * Functions to display unmanaged image files stored in S3 on node-edit forms.
 */

  //////////////////////////////////////////////////
 /// Called from mp_common_element_info_alter() ///
//////////////////////////////////////////////////

/**
 * Process function to expand a form element for a file from its default value.
 *
 * If build_form() fails to find a managed file in the db matching the fid
 * appearing in the form values, it moves all the metadata about the file to
 * #default_value. We take advantage of this and repopulate the form with file
 * info during the process step - passing it along to be rendered as normal.
 *
 * @see mp_common_element_info_alter()
 * @see file_managed_file_process()
 */
function mp_common_s3_file_process($element, &$form_state, $form) {
    // Append the '-upload' to the #id so the field label's 'for' attribute
    // corresponds with the file element.
    $original_id = $element['#id'];
    $element['#id'] .= '-upload';
    $fid = 0;
    if (!empty($element['#value']['fid'])) {
        $fid = $element['#value']['fid'];
    }
    elseif (!empty($element['#default_value']['fid'])) {
        $fid = $element['#default_value']['fid'];
    }

    // Set some default element properties.
    $element['#progress_indicator'] = empty($element['#progress_indicator']) ? 'none' : $element['#progress_indicator'];
    $element['#file'] = false;
    if ($fid && ($file = file_load($fid))) {
        $element['#file'] = $file;
    }
    elseif (isset($file)) {
        $element['#file'] = (object) array_intersect_key(
            $element['#default_value'], array(
                'fid' => 1, 'uid' => 1, 'filename' => 1, 'uri' => 1,
                'filemime' => 1, 'filesize' => 1, 'status' => 1,
                'timestamp' => 1,
            )
        );
    }
    $element['#tree'] = true;

    $ajax_settings = array(
        'path' => 'file/ajax/'.implode('/', $element['#array_parents']).'/'.$form['form_build_id']['#value'],
        'wrapper' => $original_id.'-ajax-wrapper',
        'effect' => 'fade',
        'progress' => array(
            'type' => $element['#progress_indicator'],
            'message' => $element['#progress_message'],
        ),
    );

    // Set up the buttons first since we need to check if they were clicked.
    $element['upload_button'] = array(
        '#name' => implode('_', $element['#parents']).'_upload_button',
        '#type' => 'submit',
        '#value' => t('Upload'),
        '#validate' => array(),
        '#submit' => array('file_managed_file_submit'),
        '#limit_validation_errors' => array($element['#parents']),
        '#ajax' => $ajax_settings,
        '#weight' => -5,
    );

    // Force the progress indicator for the remove button to be either 'none'
    // or 'throbber', even if the upload button is using something else.
    $ajax_settings['progress']['type'] = ($element['#progress_indicator'] == 'none') ? 'none' : 'throbber';
    $ajax_settings['progress']['message'] = null;
    $ajax_settings['effect'] = 'none';
    $element['remove_button'] = array(
        '#name' => implode('_', $element['#parents']).'_remove_button',
        '#type' => 'submit',
        '#value' => t('Remove'),
        '#validate' => array(),
        '#submit' => array('file_managed_file_submit'),
        '#limit_validation_errors' => array($element['#parents']),
        '#ajax' => $ajax_settings,
        '#weight' => -5,
    );

    $element['fid'] = array(
        '#type' => 'hidden',
        '#value' => $fid,
    );

    // Add progress bar support to the upload if possible.
    if ($element['#progress_indicator'] == 'bar' && $implementation = file_progress_implementation()) {
        $upload_progress_key = mt_rand();

        if ($implementation == 'uploadprogress') {
            $element['UPLOAD_IDENTIFIER'] = array(
                '#type' => 'hidden',
                '#value' => $upload_progress_key,
                '#attributes' => array('class' => array('file-progress')),
                // Uploadprogress extension requires this field to be at the top
                // of the form.
                '#weight' => -20,
            );
        }
        elseif ($implementation == 'apc') {
            $element['APC_UPLOAD_PROGRESS'] = array(
                '#type' => 'hidden',
                '#value' => $upload_progress_key,
                '#attributes' => array('class' => array('file-progress')),
                // Uploadprogress extension requires this field to be at the top
                // of the form.
                '#weight' => -20,
            );
        }

        // Add the upload progress callback.
        $element['upload_button']['#ajax']['progress']['path'] = 'file/progress/'.$upload_progress_key;
    }

    // The file upload field itself.
    $element['upload'] = array(
        '#name' => 'files['.implode('_', $element['#parents']).']',
        '#type' => 'file',
        '#title' => t('Choose a file'),
        '#title_display' => 'invisible',
        '#size' => $element['#size'],
        '#theme_wrappers' => array(),
        '#weight' => -10,
    );

    if ($fid && $element['#file']) {
        $element['filename'] = array(
            '#type' => 'markup',
            '#markup' => theme('file_link', array('file' => $element['#file'])).' ',
            '#weight' => -10,
        );
    }

    // Add the extension list to the page as JavaScript settings.
    if (isset($element['#upload_validators']['file_validate_extensions'][0])) {
        $extension_list = implode(',', array_filter(explode(' ', $element['#upload_validators']['file_validate_extensions'][0])));
        $element['upload']['#attached']['js'] = array(array(
            'type' => 'setting',
            'data' => array('file' => array('elements' => array('#'.$element['#id'] => $extension_list))),
        ));
    }

    // Prefix and suffix used for Ajax replacement.
    $element['#prefix'] = '<div id="'.$original_id.'-ajax-wrapper">';
    $element['#suffix'] = '</div>';

    return $element;
}

/**
 * Pre-render callback to hide display of the upload or remove controls.
 *
 * As with mp_common_s3_file_process(), we allow the pre-render function for
 * image files to read from the form's #default_value.
 *
 * @see mp_common_element_info_alter()
 * @see file_managed_file_pre_render()
 */
function mp_common_s3_file_pre_render($element) {
    if (empty($element['#value']['fid']) && empty($element['#default_value']['fid'])) {
        $element['remove_button']['#access'] = false;
    }
    else {
        $element['upload']['#access'] = false;
        $element['upload_button']['#access'] = false;
    }

    return $element;
}

/**
 * An #element_validate callback for the managed_file element.
 */
function mp_common_s3_file_validate(&$element, &$form_state) {
    // If referencing an existing file, only allow if there are existing
    // references. This prevents unmanaged files from being deleted if this
    // item were to be deleted.
    $clicked_button = end($form_state['triggering_element']['#parents']);
    $fid = empty($element['fid']['#value']) ? null : $element['fid']['#value'];
    if ($clicked_button !== 'remove_button' && $fid) {
        if ($file = file_load($fid)) {
            if ($file->status == FILE_STATUS_PERMANENT) {
                $references = file_usage_list($file);
                if (empty($references)) {
                    form_error($element, t('The file used in the !name field may not be referenced.', array('!name' => $element['#title'])));
                }
            }
        }
        // Don't throw an error if the file exists but isn't managed locally.
        elseif (!file_exists($element['#file']->uri)) {
            form_error($element, t('The file referenced by the !name field does not exist.', array('!name' => $element['#title'])));
        }
    }

    // Check required property based on the FID.
    if ($element['#required'] && empty($fid) && !in_array($clicked_button, array('upload_button', 'remove_button'))) {
        form_error($element['upload'], t('!name field is required.', array('!name' => $element['#title'])));
    }

    // Set $form_state values for the element using either the existing remote
    // file or newly-created temporary file.
    if (isset($element['#field_name']) && ($fieldname = $element['#field_name'])) {
        foreach (element_children($element) as $key) {
            if (isset($element[$key]['#value']) && ($value = $element[$key]['#value'])) {
                $form_state['values'][$fieldname][LANGUAGE_NONE][0][$key] = $value;
            }
        }
    }
}

  ///////////////////////////////////////////////////////
 /// Called from mp_common_field_widget_form_alter() ///
///////////////////////////////////////////////////////

/**
 * Custom #value_callback for the file_generic field element.
 *
 * We interrupt normal execution in order to dynamically determine the S3 path
 * to upload to based on the field's filefield_paths setting. Otherwise, the
 * filefield_paths_form_alter forces all uploads to go to the temporary file
 * system (instead of S3) prior to node_save, making them unavailable to other
 * constellation instances.
 *
 * @see file_field_widget_value()
 * @see filefield_paths_filefield_paths_process_file()
 */
function mp_common_s3_file_field_widget_value($element, $input = FALSE, $form_state) {
    if ($input) {
        // Checkboxes lose their value when empty.
        // If the display field is present make sure its unchecked value is saved.
        $field = field_widget_field($element, $form_state);
        if (empty($input['display'])) {
            $input['display'] = $field['settings']['display_field'] ? 0 : 1;
        }
    }

    // If filefield_paths is active for the given field:
    if (
        'temporary://filefield_paths' === $element['#upload_location'] &&
        ($instance = field_info_instance($element['#entity_type'], $element['#field_name'], $element['#bundle'])) &&
        isset($instance['settings']['filefield_paths'])
    ) {

        // Set widget's #upload_location to a path on S3.
        $filepath = $instance['settings']['filefield_paths']['file_path'];
        $filepath['options']['context'] = 'file_path';
        $filepath = filefield_paths_process_string($filepath['value'], array(), $filepath['options']);

        $element['#upload_location'] = 's3://' . $filepath;

        // Allow transliteration of foreign characters in the filename as well.
        $form_field_name = implode('_', $element['#parents']);
        $upload_name = &$_FILES['files']['name'][$form_field_name];
        $filename = $instance['settings']['filefield_paths']['file_name'];
        $filename['options']['context'] = 'file_name';
        $origname = drupal_basename($upload_name);
        $token_data = array('file' => (object) array('origname' => $origname));
        $filename = filefield_paths_process_string($filename['value'], $token_data, $filename['options']);

        $upload_name = str_replace($origname, $filename, $upload_name);
    }

    // We depend on the managed file element to handle uploads.
    $return = file_managed_file_value($element, $input, $form_state);

    // Ensure that all the required properties are returned even if empty.
    $return += array(
        'fid' => 0,
        'display' => 1,
        'description' => '',
    );

    return $return;
}

/**
 * Changes the label and description of an embedded file entity's title field.
 */
function mp_common_image_field_widget_process($element, &$form_state, $form) {
    // Change the title field label and description
    $element['title']['#title'] = 'Image Credit';
    $element['title']['#description'] = 'Add an image credit if required.';

    // Return the altered element
    return $element;
}

  /////////////////////////////////////
 /// Called from mp_common_theme() ///
/////////////////////////////////////

/**
 * Returns HTML for an image field widget without the filesize.
 *
 * @see theme_image_widget()
 */
function mp_common_theme_image_widget($variables) {
    $element = $variables['element'];
    $output = '<div class="image-widget form-managed-file clearfix">';

    if (isset($element['preview'])) {
        $output .= '<div class="image-preview">';
        $output .= drupal_render($element['preview']);
        $output .= '</div>';
    }

    $output .= '<div class="image-widget-data">';
    $output .= drupal_render_children($element);
    $output .= '</div>';
    $output .= '</div>';

    return $output;
}

/**
 * Returns HTML for a link to a file without the file icon and filesize.
 *
 * @see theme_file_link()
 */
function mp_common_theme_file_link($variables) {
    $file = $variables['file'];
    $url = file_create_url($file->uri);
    $options = array('attributes' => array('type' => $file->filemime));

    return '<span class="file">'.l($file->filename, $url, $options).'</span>';
}

  //////////////////////////////////////////
 /// Called from mp_common_menu_alter() ///
//////////////////////////////////////////

/**
 * Generates a local, unmanaged image derivative for content-edit forms before
 * the node is saved.
 *
 * @see _s3fs_image_style_deliver()
 */
function mp_common_image_style_deliver() {
    // Drupal's black magic calls this function with the image style as arg0,
    // the scheme as arg1, and the full path to the filename split across arg2+.
    // So we need to use PHP's version of variatic functions to get the complete
    // filename.
    $args = func_get_args();
    $style = array_shift($args);
    $scheme = array_shift($args);
    $filename = implode('/', $args);

    $valid = !empty($style);
    if (!variable_get('image_allow_insecure_derivatives', FALSE) || strpos(ltrim($filename, '\/'), 'styles/') === 0) {
        $valid = $valid && isset($_GET[IMAGE_DERIVATIVE_TOKEN]) && $_GET[IMAGE_DERIVATIVE_TOKEN] === image_style_path_token($style['name'], "$scheme://$filename");
    }
    if (!$valid) {
        return MENU_ACCESS_DENIED;
    }

    $image_uri = "$scheme://$filename";
    $derivative_uri = image_style_path($style['name'], $image_uri);

    // Confirm that the original source image exists before trying to process it.
    if (!is_file($image_uri)) {
        watchdog('s3fs', 'Source image at %source_image_path not found while trying to generate derivative image at %derivative_path.',
            array('%source_image_path' => $image_uri, '%derivative_path' => $derivative_uri)
        );
        return MENU_NOT_FOUND;
    }

    // Don't start generating the image if the derivative already exists or if
    // generation is in progress in another thread.
    $lock_name = "_s3fs_image_style_deliver:{$style['name']}:" . drupal_hash_base64($image_uri);
    if (!file_exists($derivative_uri)) {
        $lock_acquired = lock_acquire($lock_name);
        if (!$lock_acquired) {
            // Tell client to retry again in 3 seconds. Currently no browsers are known
            // to support Retry-After.
            drupal_add_http_header('Status', '503 Service Unavailable');
            drupal_add_http_header('Content-Type', 'text/html; charset=utf-8');
            drupal_add_http_header('Retry-After', 3);
            print t('Image generation in progress. Try again shortly.');
            drupal_exit();
        }
    }

    // Try to generate the image, unless another thread just did it while we were
    // acquiring the lock.
    $success = file_exists($derivative_uri);
    if (!$success) {
        // Create derivative in our local files directory.
        $derivative_uri = str_replace("$scheme://", "public://", $derivative_uri);
        $success = image_style_create_derivative($style, $image_uri, $derivative_uri);

        // If we successfully generate the derivative, wait until S3 acknolowedges
        // its existence. Otherwise, redirecting to it may cause a 403 error.
        // $success = image_style_create_derivative($style, $image_uri, $derivative_uri) &&
        //   file_stream_wrapper_get_instance_by_scheme('s3')->waitUntilFileExists($derivative_uri);
    }

    if (!empty($lock_acquired)) {
        lock_release($lock_name);
    }

    if ($success) {
        // Perform a 302 Redirect to the new image derivative in S3.
        drupal_goto(file_create_url($derivative_uri));
    }
    else {
        watchdog('S3 File System', 'Unable to generate an image derivative at %path.', array('%path' => $derivative_uri));
        drupal_add_http_header('Status', '500 Internal Server Error');
        drupal_add_http_header('Content-Type', 'text/html; charset=utf-8');
        print t('Error generating image.');
        drupal_exit();
    }
}

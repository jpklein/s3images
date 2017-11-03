<?php
/**
 * @file
 * Drupal stream wrapper implementation for S3 File System.
 */

/**
 * Prevents file info from being written to the local database.
 */
class MpS3fsStreamWrapper extends S3fsStreamWrapper {

    /**
     * Write an object's (and its ancestor folders') metadata to the cache.
     *
     * @param array $metadata
     *   An associative array of file metadata in this format:
     *     'uri' => The full URI of the file, including the scheme.
     *     'filesize' => The size of the file, in bytes.
     *     'timestamp' => The file's create/update timestamp.
     *     'dir' => A boolean indicating whether the object is a directory.
     *
     * @throws
     *   Exceptions which occur in the database call will percolate.
     */
    protected function _write_cache($metadata) {
        $this->_debug("_write_cache({$metadata['uri']}) called.", TRUE);

        // Since public:///blah.jpg and public://blah.jpg refer to the same file
        // (a file named blah.jpg at the root of the file system), we'll sometimes
        // receive files with a /// in their URI. This messes with our caching
        // scheme, though, so we need to remove the extra /.
        if (strpos($metadata['uri'], 'public:///') === 0) {
            $metadata['uri'] = preg_replace('^public://[/]+^', 'public://', $metadata['uri']);
        }
        else if (strpos($metadata['uri'], 'private:///') === 0) {
            $metadata['uri'] = preg_replace('^private://[/]+^', 'private://', $metadata['uri']);
        }

        // db_merge('s3fs_file')
        //   ->key(array('uri' => $metadata['uri']))
        //   ->fields($metadata)
        //   ->execute();

        // Clear this URI from the Drupal cache, to ensure the next read isn't
        // from a stale cache entry.
        $cid = S3FS_CACHE_PREFIX . $metadata['uri'];
        cache_clear_all($cid, S3FS_CACHE_BIN);

        $dirname = drupal_dirname($metadata['uri']);
        // If this file isn't in the root directory, also write this file's
        // ancestor folders to the cache.
        if (file_uri_target($dirname) != '') {
            $this->mkdir($dirname, NULL, STREAM_MKDIR_RECURSIVE);
        }
    }
}

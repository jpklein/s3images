# Drupal 7 S3 Image File Handling

## Problem
The s3fs contrib module was saving a copy of files to the local db in order to display thumbnail previews, which placed an unnecessary burden on our persistence and caching layers and essentially nullified the benefits of hosting assets in s3.

## Solution
Using hooks and class overrides, I was able to exploit quirks in the way that drupal processes asset uploads and renders images on entity forms to rely solely on remote copies of image files.

This sample is a subset of code from a shared module containing several functions common to a constellation of Drupal installs. The solution is implemented in four files: 
```
sites/all/modules/custom/mp_common/mp_common.info
sites/all/modules/custom/mp_common/mp_common.module
sites/all/modules/custom/mp_common/includes/mp_common.images.inc
sites/all/modules/custom/mp_common/includes/mp_common.stream_wrapper.inc
```

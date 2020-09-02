from helper import ExitCode, SystemTest
import os
import unittest

class CreateTest(SystemTest):

  def test_info(self):
    self.assertInfo('btoep-create', [
      '--dataset', '--index-path', '--lockfile-path',
      '--size'
    ])

  def assertCreate(self, dataset, size = None):
    args = ['btoep-create', '--dataset', dataset]
    if size is not None:
      args.append('--size=' + str(size))
    self.cmd(args)
    effective_size = 0 if size is None else size
    self.assertEqual(effective_size, os.path.getsize(dataset))
    self.assertEqual(self.readIndex(dataset), b'')

  def test_create(self):
    # Create a new, empty dataset.
    self.assertCreate(self.reserveDataset())

    # Create a new dataset with a given size.
    self.assertCreate(self.reserveDataset(), size = 10000)

  def test_fs_error(self):
    # Test that the command fails if the dataset already exists.
    dataset = self.createDataset(b'', b'')
    self.assertErrorMessage(
        ['btoep-create', '--dataset', dataset],
        message = 'System input/output error',
        has_ext_message = True,
        lib_error_name = 'ERR_INPUT_OUTPUT',
        lib_error_code = '1',
        sys_error_name = 'ERROR_FILE_EXISTS' if self.isWindows else 'EEXIST',
        sys_error_code = '80' if self.isWindows else '17')

    # Test that the command fails if the data file exists.
    dataset = self.createDataset(b'foo', None)
    self.assertErrorMessage(
        ['btoep-create', '--dataset', dataset],
        message = 'System input/output error',
        has_ext_message = True,
        lib_error_name = 'ERR_INPUT_OUTPUT',
        lib_error_code = '1',
        sys_error_name = 'ERROR_FILE_EXISTS' if self.isWindows else 'EEXIST',
        sys_error_code = '80' if self.isWindows else '17')

    # This should not have modified the existing data file, or created the index
    # file.
    self.assertEqual(self.readDataset(dataset), b'foo')
    self.assertIsNone(self.readIndex(dataset))

    # Test that the command fails if the index file exists.
    dataset = self.createDataset(None, b'bar')
    self.assertErrorMessage(
        ['btoep-create', '--dataset', dataset],
        message = 'System input/output error',
        has_ext_message = True,
        lib_error_name = 'ERR_INPUT_OUTPUT',
        lib_error_code = '1',
        sys_error_name = 'ERROR_FILE_EXISTS' if self.isWindows else 'EEXIST',
        sys_error_code = '80' if self.isWindows else '17')

    # This should not have created the data file, or modified the existing index
    # file.
    self.assertIsNone(self.readDataset(dataset))
    self.assertEqual(self.readIndex(dataset), b'bar')

if __name__ == '__main__':
  unittest.main()

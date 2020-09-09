from helper import ExitCode, SystemTest
from tempfile import NamedTemporaryFile
import unittest

class AddTest(SystemTest):

  def test_info(self):
    self.assertInfo([
      '--dataset', '--index-path', '--lockfile-path',
      '--offset', '--on-conflict', '--source'
    ])

  def test_add(self):
    all_data = ((b'\xaa' * 256) + (b'\xbb' * 256) + (b'\xcc' * 256)) * 3

    # Create a new dataset.
    dataset = self.reserveDataset()
    self.cmd(['--dataset', dataset, '--offset=512'],
             input = all_data[512:640])
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(len(self.readDataset(dataset)), 640)
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f')

    # Add another, separate range.
    self.cmd(['--dataset', dataset, '--offset=1024'],
             input = all_data[1024:1152])
    self.assertEqual(self.readDataset(dataset)[512:640], all_data[512:640])
    self.assertEqual(self.readDataset(dataset)[1024:1152], all_data[1024:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\x7f\xff\x02\x7f')

    # Add a range that merges both existing ranges, but does not overlap.
    self.cmd(['--dataset', dataset, '--offset=640'],
             input = all_data[640:1024])
    self.assertEqual(self.readDataset(dataset)[512:1152], all_data[512:1152])
    self.assertEqual(self.readIndex(dataset), b'\x80\x04\xff\x04')

    # Now add a superset.
    self.cmd(['--dataset', dataset, '--offset=256'],
             input = all_data[256:1280])
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(self.readIndex(dataset), b'\x80\x02\xff\x07')

    # Create conflicting data by swapping every single bit.
    conflicting_data = bytes([v ^ 0xff for v in all_data])

    # Adding conflicting data with the default/error behavior should not change
    # existing ranges or the file size, and should not modify the index.
    self.assertErrorMessage(['--dataset', dataset, '--offset=0'],
                            input = conflicting_data,
                            message = 'Data conflicts with existing data',
                            lib_error_name = 'ERR_DATA_CONFLICT',
                            lib_error_code = '5')

    self.assertErrorMessage(['--dataset', dataset, '--offset=0',
                             '--on-conflict=error'],
                            input = conflicting_data,
                            message = 'Data conflicts with existing data',
                            lib_error_name = 'ERR_DATA_CONFLICT',
                            lib_error_code = '5')

    # Ensure dataset and index are unchanged.
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(len(self.readDataset(dataset)), 1280)
    self.assertEqual(self.readIndex(dataset), b'\x80\x02\xff\x07')

    self.cmd(['--dataset', dataset, '--offset=0', '--on-conflict=keep'],
             input = conflicting_data)
    self.assertEqual(self.readDataset(dataset)[0:256], conflicting_data[0:256])
    self.assertEqual(self.readDataset(dataset)[256:1280], all_data[256:1280])
    self.assertEqual(self.readDataset(dataset)[1280:], conflicting_data[1280:])
    self.assertEqual(len(self.readDataset(dataset)), len(conflicting_data))
    self.assertEqual(self.readIndex(dataset), b'\x00\xff\x11')

    self.cmd(['--dataset', dataset, '--offset=0', '--on-conflict=overwrite'],
             input = all_data)
    self.assertEqual(self.readDataset(dataset), all_data)
    self.assertEqual(len(self.readDataset(dataset)), len(all_data))
    self.assertEqual(self.readIndex(dataset), b'\x00\xff\x11')

  def test_add_from_file(self):
    # Create a new dataset from a file.
    path = self.createTempTestFile(b'Hello world\n')
    dataset = self.reserveDataset()
    self.cmd(['--dataset', dataset, '--source', path, '--offset=0'])
    self.assertEqual(self.readDataset(dataset), b'Hello world\n')
    self.assertEqual(self.readIndex(dataset), b'\x00\x0b')

    # Now do the same with Windows-style line endings.
    path = self.createTempTestFile(b'Hello world\r\n')
    dataset = self.reserveDataset()
    self.cmd(['--dataset', dataset, '--source', path, '--offset=0'])
    self.assertEqual(self.readDataset(dataset), b'Hello world\r\n')
    self.assertEqual(self.readIndex(dataset), b'\x00\x0c')

  def test_fs_error(self):
    # Test that the command fails if only the data file is missing
    dataset = self.createDataset(None, b'foo')
    self.assertErrorMessage(
        ['--dataset', dataset, '--offset=0'],
        message = 'System input/output error',
        has_ext_message = True,
        lib_error_name = 'ERR_INPUT_OUTPUT',
        lib_error_code = '1',
        sys_error_name = 'ERROR_FILE_EXISTS' if self.isWindows else 'EEXIST',
        sys_error_code = '80' if self.isWindows else '17')

    # This should not have created the data file, or modified the existing index
    # file.
    self.assertIsNone(self.readDataset(dataset))
    self.assertEqual(self.readIndex(dataset), b'foo')

    # Test that the command fails if only the index file is missing
    dataset = self.createDataset(b'bar', None)
    self.assertErrorMessage(
        ['--dataset', dataset, '--offset=0'],
        message = 'System input/output error',
        has_ext_message = True,
        lib_error_name = 'ERR_INPUT_OUTPUT',
        lib_error_code = '1',
        sys_error_name = 'ERROR_FILE_NOT_FOUND' if self.isWindows else 'ENOENT',
        sys_error_code = '2')

    # This should not have modified the existing data file, or created the index
    # file.
    self.assertEqual(self.readDataset(dataset), b'bar')
    self.assertIsNone(self.readIndex(dataset))

  def test_invalid_source(self):
    # Test --source with a directory.
    empty_dir = self.createTempTestDir()
    dataset = self.reserveDataset()
    self.assertErrorMessage(
        ['--dataset', dataset, '--offset=0', '--source', empty_dir],
        message = True,
        sys_error_name = 'EACCES' if self.isWindows else 'EISDIR',
        sys_error_code = '13' if self.isWindows else '21')

    # Test --source with a file that does not exist.
    not_a_file = empty_dir + '/foo'
    dataset = self.reserveDataset()
    self.assertErrorMessage(
        ['--dataset', dataset, '--offset=0', '--source', not_a_file],
        message = True,
        sys_error_name = 'ENOENT',
        sys_error_code = '2')
    self.assertIsNone(self.readDataset(dataset))
    self.assertIsNone(self.readIndex(dataset))

if __name__ == '__main__':
  unittest.main()

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
    stderr = self.cmd_stderr(['btoep-create', '--dataset', dataset],
                             expected_returncode = ExitCode.APP_ERROR)
    self.assertTrue(stderr.startswith('Error: System input/output error: '))

if __name__ == '__main__':
  unittest.main()

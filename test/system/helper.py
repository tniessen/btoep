import enum
import io
import os
import platform
import re
import shutil
import subprocess
import tempfile
import unittest

class ExitCode(enum.Enum):
  SUCCESS = 0
  NO_RESULT = 1
  USAGE_ERROR = 2
  APP_ERROR = 3

def tostring(b):
  return io.StringIO(b.decode(), None).getvalue()

class SystemTest(unittest.TestCase):

  def setUp(self):
    self.dir = tempfile.mkdtemp()
    self.n_datasets = 0
    self.isWindows = platform.system() == 'Windows'

  def tearDown(self):
    shutil.rmtree(self.dir)

  def reserveDataset(self):
    path = os.path.join(self.dir, 'dataset_' + str(self.n_datasets))
    self.n_datasets += 1
    return path

  def createTempTestFile(self, data):
    with tempfile.NamedTemporaryFile(delete=False, dir=self.dir) as file:
      file.write(data)
      return file.name

  def createTempTestDir(self):
    return tempfile.mkdtemp(dir = self.dir)

  def createDataset(self, data, index_data):
    path = self.reserveDataset()
    if data is not None:
      with open(path, 'wb') as dataset:
        dataset.write(data)
    if index_data is not None:
      with open(path + '.idx', 'wb') as index:
        index.write(index_data)
    return path

  def readDataset(self, path):
    try:
      with open(path, 'rb') as dataset:
        return dataset.read()
    except FileNotFoundError:
      return None

  def readIndex(self, path):
    try:
      with open(path + '.idx', 'rb') as dataset:
        return dataset.read()
    except FileNotFoundError:
      return None

  def assertOutputIsEqual(self, actual, expected):
    if isinstance(expected, str):
      self.assertEqual(tostring(actual), expected)
    else:
      self.assertEqual(actual, expected)

  def arg0(self):
    name = re.sub(r'Test$', '', type(self).__name__)
    repl = lambda match: '-' + match.group(0).lower()
    return 'btoep' + re.sub(r'[A-Z]', repl, name)

  def cmd(self, args,
          expected_returncode=ExitCode.SUCCESS,
          expected_stdout=b'',
          expected_stderr=b'',
          **kwargs):
    argv = [self.arg0()] + args
    result = subprocess.run(argv, capture_output=True, timeout=10, **kwargs)
    self.assertEqual(ExitCode(result.returncode), expected_returncode)
    if expected_stdout is not None:
      self.assertOutputIsEqual(result.stdout, expected_stdout)
    if expected_stderr is not None:
      self.assertOutputIsEqual(result.stderr, expected_stderr)
    return result

  def cmd_stdout(self, args, text=False, **kwargs):
    result = self.cmd(args, expected_stdout=None, **kwargs)
    return (tostring(result.stdout) if text else result.stdout)

  def cmd_stderr(self, args, text=True, **kwargs):
    result = self.cmd(args, expected_stderr=None, **kwargs)
    return (tostring(result.stderr) if text else result.stderr)

  def assertErrorMessage(self, args,
                         message, has_ext_message=False,
                         lib_error_name=None, lib_error_code=None,
                         sys_error_name=None, sys_error_code=None,
                         **kwargs):
    stderr = self.cmd_stderr(args, expected_returncode = ExitCode.APP_ERROR,
                             **kwargs)
    regex = r"^Error: ([^:\n]+)(: ([^:\n]+))?\n\n(([^:\n]+: [^\n]+\n)*)$"
    match = re.fullmatch(regex, stderr)
    self.assertIsNotNone(match)
    if message is not True:
      self.assertEqual(match.group(1), message)
    self.assertEqual(match.group(3) is not None, has_ext_message)
    props = {}
    for line in match.group(4).splitlines():
      (key, value) = line.split(': ')
      props[key] = value
    self.assertEqual(props.get('Library error name'), lib_error_name)
    self.assertEqual(props.get('Library error code'), lib_error_code)
    self.assertEqual(props.get('System error name'), sys_error_name)
    self.assertEqual(props.get('System error code'), sys_error_code)

  def assertInfo(self, options):
    version = self.cmd_stdout(['--version'], text=True)
    self.assertTrue(version.startswith(self.arg0() + ' '))

    help = self.cmd_stdout(['--help'], text=True)
    self.assertTrue(help.startswith('Usage: ' + self.arg0() + ' [options]\n'))
    for option in options + ['--version', '--help']:
      self.assertIn('\n' + option, help)
    # By convention, the information should not exceed 80 columns.
    for line in help.splitlines():
      self.assertLessEqual(len(line), 80)

import { BackupFns } from '../../backups';
import { DropboxOptions } from 'dropbox';
export default function mkBackupFn(opts: DropboxOptions, path: string, rdbFilePath: string): Promise<BackupFns>;

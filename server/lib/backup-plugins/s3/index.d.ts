import { BackupFns } from '../../backups';
declare type S3Opts = {
    config: {
        accessKeyId: string;
        secretAccessKey: string;
    };
    endpoint: string;
    bucketName: string;
    backupRetentionInDays?: number;
};
export default function mkBackupFn(opts: S3Opts): Promise<BackupFns>;
export {};

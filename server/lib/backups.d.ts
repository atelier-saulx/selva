export declare type BackupFns = {
    sendBackup: SendBackup;
    loadBackup: LoadBackup;
};
export declare type SendBackup = (rdbFilePath: string) => Promise<void>;
export declare type LoadBackup = (rdbFilePath: string, rdbLastModified?: Date) => Promise<void>;
export declare function loadBackup(redisDir: string, backupFns: BackupFns): Promise<void>;
export declare function saveAndBackUp(redisDir: string, redisPort: number, backupFns: BackupFns): Promise<void>;
export declare function scheduleBackups(redisDir: string, redisPort: number, intervalInMinutes: number, backupFns: BackupFns): Promise<void>;

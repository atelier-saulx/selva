import { BackupFns } from './backups';
declare type Service = {
    port: number;
    host: string;
};
declare type FnStart = {
    port?: number | Promise<number>;
    service?: Service | Promise<Service>;
    replica?: Service | Promise<Service>;
    modules?: string[];
    verbose?: boolean;
    backups?: {
        loadBackup?: boolean;
        scheduled?: {
            intervalInMinutes: number;
        };
        backupFns: BackupFns | Promise<BackupFns>;
    };
    subscriptions?: boolean;
};
declare type SelvaServer = {
    on: (type: 'log' | 'data' | 'close' | 'error', cb: (data: any) => void) => void;
    destroy: () => Promise<void>;
    backup: () => Promise<void>;
    openSubscriptions: () => Promise<void>;
    closeSubscriptions: () => void;
};
export declare const start: ({ port: portOpt, service, modules, replica, verbose, backups, subscriptions }: FnStart) => Promise<SelvaServer>;
export {};

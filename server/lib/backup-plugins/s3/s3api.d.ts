import aws from 'aws-sdk';
export declare type S3Api = {
    getBuckets: () => Promise<aws.S3.Bucket[]>;
    createBucket: (bucketName: string, acl: string) => Promise<void>;
    ensureBucket: (bucketName: string, acl: string) => Promise<void>;
    listObjects: (bucketName: string) => Promise<aws.S3.ObjectList>;
    getObject: (bucketName: string, filepath: string) => Promise<aws.S3.Body>;
    deleteObject: (bucketName: string, filepath: string) => Promise<void>;
    storeFile: (bucketName: string, destFilepath: string, sourceFilepath: string) => Promise<void>;
};
export declare function createApi(opts: {
    accessKeyId: string;
    secretAccessKey: string;
}, endpoint: string): S3Api;

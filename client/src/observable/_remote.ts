import { GetOptions } from '../get'

import { Connection } from '../connection'

import {
    NEW_SUBSCRIPTION,
    SUBSCRIPTIONS,
    REMOVE_SUBSCRIPTION,
    CACHE
} from '../constants'
import { SelvaClient, Observable } from '..'

export function start(
    selvaClient: SelvaClient,
    connection: Connection,
    channel: string,
    getOptions: GetOptions
) {
    connection.command({
        command: 'hsetnx',
        args: [SUBSCRIPTIONS, channel, JSON.stringify(getOptions)],
        id: selvaClient.selvaId
    })
    connection.command({
        command: 'sadd',
        args: [channel, connection.uuid],
        id: selvaClient.selvaId
    })
    connection.command({
        command: 'publish',
        args: [
            NEW_SUBSCRIPTION,
            JSON.stringify({ client: connection.uuid, channel })
        ],
        id: selvaClient.selvaId
    })

    connection.subscribe(channel, selvaClient.selvaId)
}

export function stop(
    selvaClient: SelvaClient,
    connection: Connection,
    channel: string,
) {
    connection.command({
        command: 'srem',
        args: [channel, connection.uuid],
        id: selvaClient.selvaId
    })
    connection.command({
        command: 'publish',
        args: [
            REMOVE_SUBSCRIPTION,
            JSON.stringify({ client: this.uuid, channel })
        ],
        id: selvaClient.selvaId
    })
    connection.unsubscribe(channel, selvaClient.selvaId)
}


/*
const parseError = (obj: { [key: string]: any }) => {
    const err = obj.payload && obj.payload.___$error___
    if (typeof err === 'string') {
        return new Error(err)
    } else if (err.message) {
        return new Error(err.message)
    } else if (err.command) {
        const { command, args, code } = err
        if (command === 'EVALSHA') {
            return new Error(`Lua error ${args.slice(3).join(', ')}`)
        }
        return new Error(`${command} ${args.join(', ')} ${code}`)
    }
    return new Error('Unkown error')
}


*/

//. this must be done better

// export const getValueObservable = (
//     observable: Observable,
//     selvaClient: SelvaClient,
//     connection: Connection,
//     channel: string
// ) => {
//     connection.command({
//         command: 'hmget',
//         id: selvaClient.selvaId,
//         args: [CACHE, channel, channel + '_version'],
//         resolve: ([data, version]) => {
//             if (data) {

//                 const obj = JSON.parse(data)
//                 // obj.version = version
//                 if (obj.payload && obj.payload.___$error___) {
//                     observable.emitError(parseError(obj))
//                 } else {
//                     // obj.payload
//                     observable.emitUpdate(parseError(obj))
//                 }
//             } else {
//                 resolve()
//             }
//         },
//         reject
//     })
// }

// export const getValue = (
//     selvaClient: SelvaClient,
//     connection: Connection,
//     channel: string
// ) =>
//     new Promise((resolve, reject) => {
//         connection.command({
//             command: 'hmget',
//             id: selvaClient.selvaId,
//             args: [CACHE, channel, channel + '_version'],
//             resolve: ([data, version]) => {
//                 if (data) {
//                     const obj = JSON.parse(data)
//                     obj.version = version
//                     if (obj.payload && obj.payload.___$error___) {
//                         reject(parseError(obj))
//                     } else {
//                         resolve(obj)
//                     }
//                 } else {
//                     resolve()
//                 }
//             },
//             reject
//         })
//     })

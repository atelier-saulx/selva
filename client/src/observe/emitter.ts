import { EventEmitter } from 'events'
import { GetOptions } from '../get'

class ObserverEmitter extends EventEmitter {
  public count: number = 0
  public isSent: boolean = false // maybe not nessecary
  public getOptions: GetOptions
  public channel: string
  public validationError?: Error
  public isRemoved: boolean = false
  constructor(getOptions: GetOptions, channel: string) {
    super()
    this.channel = channel
    this.getOptions = getOptions
    this.setMaxListeners(1e4)
    // isSent
  }
}

// make this nicer
export default ObserverEmitter

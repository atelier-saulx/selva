type SubscriptionObserver<T> = {
  next: (value: T) => void
  error: (err: Error) => void
  complete: () => void
  closed: boolean // implemented as getter
}

type Observer<T> = {
  start?: (subscription: Subscription<T>) => void
  next?: (value: T) => void
  error?: (err: Error) => void
  complete?: () => void
}

type SubscriberFn<T> = (
  observer: SubscriptionObserver<T>
) => (() => void) | Subscription<T>

class Subscription<T> {
  private observe: SubscriberFn<T>
  public observer: Observer<T>
  private cleanup: () => void

  constructor(observe: SubscriberFn<T>, observer: Observer<T>) {
    this.observe = observe
    this.observer = observer

    if (this.observer.start) {
      this.observer.start(this)

      // if alreadfy closed
      if (this.closed) {
        return
      }
    }

    const subObserver: SubscriptionObserver<T> = {
      next: !observer.next
        ? () => {}
        : (value: any) => {
            if (this.closed) {
              return
            }

            observer.next(value)
          },
      error: (e: Error) => {
        if (this.closed) {
          return
        }

        if (observer.error) {
          observer.error(e)
        }

        this.runCleanup()
      },
      complete: () => {
        if (this.closed) {
          return
        }

        if (observer.complete) {
          observer.complete()
        }

        this.runCleanup()
      },
      get closed() {
        return this.closed
      }
    }

    try {
      const cleanup = this.observe(subObserver)
      this.cleanup =
        typeof cleanup === 'function'
          ? cleanup
          : cleanup.unsubscribe.bind(cleanup)

      if (this.closed) {
        this.runCleanup()
        return
      }
    } catch (e) {
      observer.error(e)
      return
    }
  }

  private runCleanup(): void {
    if (!this.cleanup) {
      return
    }

    let f = this.cleanup
    this.cleanup = undefined // drop reference to ensure one call
    try {
      f()
    } catch (e) {
      console.error(`Error cleaning up subscription`, e)
    }
  }

  unsubscribe(): void {
    if (this.closed) {
      return
    }

    this.observer = undefined
    this.runCleanup()
  }

  get closed(): boolean {
    return this.observer === undefined
  }
}

export default class Observable<T> {
  private observe: SubscriberFn<T>

  constructor(observe: SubscriberFn<T>) {
    this.observe = observe
  }

  // [Symbol.observable](): Observable

  subscribe(
    onNext: ((x: T) => void) | Observer<T>,
    onError?: (e: Error) => void,
    onComplete?: () => void
  ): Subscription<T> {
    if (typeof onNext === 'function') {
      return this.subscribe({
        next: onNext,
        error: onError,
        complete: onComplete
      })
    }

    return new Subscription(this.observe, onNext)
  }
}

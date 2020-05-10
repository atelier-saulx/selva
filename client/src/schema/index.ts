import { SelvaClient } from '..'

// export function subscribeSchema(client: SelvaClient): Observable<Schema> {}

// class SchemaClass {
//   // public schemaObservable: Observable<Schema>

//   // subscribeSchema() {
//   //   console.log('SUBSCRIBE SCHEMA')
//   //   if (this.schemaObservable) {
//   //     return this.schemaObservable
//   //   }

//   //   const obs = this.redis.subscribe(
//   //     `___selva_subscription:schema_update`,
//   //     {}
//   //   )

//   //   this.schemaObservable = new Observable<Schema>(observe => {
//   //     const sub = obs.subscribe({
//   //       next: (_x: any) => {
//   //         observe.next(_x)
//   //       },
//   //       error: observe.error,
//   //       complete: observe.complete
//   //     })

//   //     return <any>sub
//   //   })

//   //   return this.schemaObservable
//   // }
// }

// export default SchemaClass

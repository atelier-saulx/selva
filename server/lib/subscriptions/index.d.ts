export default class SubscriptionManager {
    private refreshSubscriptionsTimeout;
    private lastRefreshed;
    private lastModifyEvent;
    private subscriptions;
    private subscriptionsByField;
    private refsById;
    private lastResultHash;
    private lastHeartbeat;
    private client;
    private sub;
    private pub;
    heartbeats(): void;
    attach(port: number): Promise<void>;
    detach(): void;
    get closed(): boolean;
    private sendUpdate;
    private refreshSubscription;
    private recalculateUpdates;
    private refreshSubscriptions;
}

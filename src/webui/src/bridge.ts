/**
 * Harken Search WebUI Bridge - JS ↔ C++ 双向通信封装
 *
 * 协议格式：
 *   JS→C++ 请求：{ "id": number, "method": string, "params": object }
 *   C++→JS 响应：{ "id": number, "result": any } 或 { "id": number, "error": string }
 *   C++→JS 事件：{ "event": string, "data": any }
 */

// ============================================================================
// Types
// ============================================================================

/** WebView2 bridge interface (injected by the host) */
interface ChromeWebView {
  postMessage(message: string): void;
  addEventListener(type: 'message', listener: (event: MessageEvent<string>) => void): void;
  removeEventListener(type: 'message', listener: (event: MessageEvent<string>) => void): void;
}

declare global {
  interface Window {
    chrome?: {
      webview?: ChromeWebView;
    };
  }
}

/** Pending RPC request */
interface PendingRequest {
  resolve: (result: unknown) => void;
  reject: (error: Error) => void;
  timer: ReturnType<typeof setTimeout>;
}

/** Event listener */
type EventCallback = (data: unknown) => void;

// ============================================================================
// Bridge Singleton
// ============================================================================

class AuroBridge {
  private nextId = 1;
  private pendingRequests = new Map<number, PendingRequest>();
  private eventListeners = new Map<string, Set<EventCallback>>();
  private connected = false;
  private readonly DEFAULT_TIMEOUT = 10000; // 10s

  constructor() {
    this.setupListener();
  }

  /** Check if running inside WebView2 */
  get isWebView2(): boolean {
    return !!(window.chrome?.webview);
  }

  // ---- RPC Invoke ----

  /**
   * 调用 C++ 端注册的方法
   * @param method 方法名（如 "ping", "db.search"）
   * @param params 参数对象
   * @param timeout 超时毫秒数（默认 10s）
   * @returns Promise 返回 C++ 端的结果
   */
  invoke<T = unknown>(method: string, params: Record<string, unknown> = {}, timeout?: number): Promise<T> {
    return new Promise<T>((resolve, reject) => {
      if (!this.isWebView2) {
        // 开发模式下模拟响应
        console.warn(`[AuroBridge] Not in WebView2, mock invoke: ${method}`, params);
        reject(new Error('Not running inside WebView2'));
        return;
      }

      const id = this.nextId++;
      const timeoutMs = timeout ?? this.DEFAULT_TIMEOUT;

      // 超时处理
      const timer = setTimeout(() => {
        this.pendingRequests.delete(id);
        reject(new Error(`RPC timeout: ${method} (id=${id}, ${timeoutMs}ms)`));
      }, timeoutMs);

      this.pendingRequests.set(id, {
        resolve: resolve as (result: unknown) => void,
        reject,
        timer,
      });

      // 发送 JSON-RPC 请求
      const message = JSON.stringify({ id, method, params });
      window.chrome!.webview!.postMessage(message);
    });
  }

  // ---- Event Subscription ----

  /**
   * 订阅 C++ 端推送的事件
   * @param event 事件名（如 "playback.stateChanged"）
   * @param callback 回调函数
   * @returns 取消订阅的函数
   */
  onEvent(event: string, callback: EventCallback): () => void {
    if (!this.eventListeners.has(event)) {
      this.eventListeners.set(event, new Set());
    }
    this.eventListeners.get(event)!.add(callback);

    // 返回取消订阅函数
    return () => {
      const listeners = this.eventListeners.get(event);
      if (listeners) {
        listeners.delete(callback);
        if (listeners.size === 0) {
          this.eventListeners.delete(event);
        }
      }
    };
  }

  // ---- Internal ----

  private setupListener(): void {
    if (!this.isWebView2) {
      console.info('[AuroBridge] Not in WebView2 environment, bridge in mock mode.');
      return;
    }

    window.chrome!.webview!.addEventListener('message', (event: MessageEvent<string>) => {
      this.handleMessage(event.data);
    });

    this.connected = true;
    console.info('[AuroBridge] Connected to WebView2 host.');
  }

  private handleMessage(raw: string): void {
    let msg: Record<string, unknown>;
    try {
      msg = JSON.parse(raw);
    } catch {
      console.warn('[AuroBridge] Failed to parse message:', raw);
      return;
    }

    // 检查是否是 RPC 响应 (有 id 字段)
    if (typeof msg.id === 'number') {
      const pending = this.pendingRequests.get(msg.id);
      if (!pending) {
        console.warn(`[AuroBridge] No pending request for id=${msg.id}`);
        return;
      }

      this.pendingRequests.delete(msg.id);
      clearTimeout(pending.timer);

      if ('error' in msg) {
        pending.reject(new Error(String(msg.error)));
      } else {
        pending.resolve(msg.result);
      }
      return;
    }

    // 检查是否是事件推送 (有 event 字段)
    if (typeof msg.event === 'string') {
      const listeners = this.eventListeners.get(msg.event);
      if (listeners) {
        for (const cb of listeners) {
          try {
            cb(msg.data);
          } catch (err) {
            console.error(`[AuroBridge] Event handler error for "${msg.event}":`, err);
          }
        }
      }
      return;
    }

    console.warn('[AuroBridge] Unknown message format:', msg);
  }
}

// ---- Singleton Export ----

/** 全局 Bridge 实例 */
export const bridge = new AuroBridge();

/** 便捷方法：调用 C++ 方法 */
export const invoke = bridge.invoke.bind(bridge);

/** 便捷方法：订阅事件 */
export const onEvent = bridge.onEvent.bind(bridge);

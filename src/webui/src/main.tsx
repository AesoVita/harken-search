/**
 * Harken Search WebUI - 入口文件
 */

import React from 'react';
import { createRoot } from 'react-dom/client';
import App from './App';
import './styles/harken-search-tokens.generated.css';
import './styles/flash-0225.css';
import DesignReferencePage from './components/design-reference/DesignReferencePage';
import ImguiReferencePage from './components/design-reference/ImguiReferencePage';
import RuntimeTwinPage from './components/runtime-twin/RuntimeTwinPage';
import DesktopAnnotationPage from './components/desktop-annotation/DesktopAnnotationPage';

const rootElement = document.getElementById('root');
if (rootElement) {
  const root = createRoot(rootElement);
  root.render(
    <React.StrictMode>
      {new URLSearchParams(window.location.search).has('desktop-annotation')
        ? <DesktopAnnotationPage />
        : new URLSearchParams(window.location.search).has('runtime-twin')
        ? <RuntimeTwinPage />
        : new URLSearchParams(window.location.search).has('imgui-reference')
        ? <ImguiReferencePage />
        : new URLSearchParams(window.location.search).has('minimal')
          ? <DesignReferencePage />
          : <App />}
    </React.StrictMode>
  );
}

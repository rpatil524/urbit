import Mousetrap from 'mousetrap';
import shallow from 'zustand/shallow';
import 'mousetrap-global-bind';
import * as React from 'react';
import Helmet from 'react-helmet';
import { Router, withRouter } from 'react-router-dom';
import styled, { ThemeProvider } from 'styled-components';
import FingerprintJS from '@fingerprintjs/fingerprintjs';
import gcpManager from '~/logic/lib/gcpManager';
import { svgDataURL } from '~/logic/lib/util';
import history from '~/logic/lib/history';
import { favicon } from '~/logic/state/contact';
import useLocalState, { selectLocalState } from '~/logic/state/local';
import useSettingsState, { selectSettingsState, SettingsState } from '~/logic/state/settings';
import useGraphState, { GraphState } from '~/logic/state/graph';
import { ShortcutContextProvider } from '~/logic/lib/shortcutContext';

import ErrorBoundary from '~/views/components/ErrorBoundary';
import './apps/chat/css/custom.css';
import Omnibox from './components/leap/Omnibox';
import StatusBar from './components/StatusBar';
import './css/fonts.css';
import './css/indigo-static.css';
import { Content } from './landscape/components/Content';
import './landscape/css/custom.css';
import { bootstrapApi } from '~/logic/api/bootstrap';
import { uxToHex } from '@urbit/api';
import { useThemeWatcher } from '~/logic/lib/useThemeWatcher';

function ensureValidHex(color) {
  if (!color)
    return '#000000';

  const isUx = color.startsWith('0x');
  const parsedColor = isUx ? uxToHex(color) : color;

  return parsedColor.startsWith('#') ? parsedColor : `#${parsedColor}`;
}

const getId = async () => {
  const fpPromise = FingerprintJS.load();
  const fp = await fpPromise;
  const result = await fp.get();
  return result.visitorId;
};

interface RootProps {
  display: SettingsState['display'];
}

const Root = styled.div<RootProps>`
  font-family: ${p => p.theme.fonts.sans};
  height: 100%;
  width: 100%;
  padding-left: env(safe-area-inset-left, 0px);
  padding-right: env(safe-area-inset-right, 0px);
  padding-top: env(safe-area-inset-top, 0px);
  padding-bottom: env(safe-area-inset-bottom, 0px);

  margin: 0;
  ${p => p.display.backgroundType === 'url' ? `
    background-image: url('${p.display.background}');
    background-size: cover;
    ` : p.display.backgroundType === 'color' ? `
    background-color: ${ensureValidHex(p.display.background)};
    ` : `background-color: ${p.theme.colors.white};`
  }
  display: flex;
  flex-flow: column nowrap;
  touch-action: none;

  * {
    scrollbar-width: thin;
    scrollbar-color: ${ p => p.theme.colors.gray } transparent;
  }

  /* Works on Chrome/Edge/Safari */
  *::-webkit-scrollbar {
    width: 6px;
    height: 6px;
  }
  *::-webkit-scrollbar-track {
    background: transparent;
  }
  *::-webkit-scrollbar-thumb {
    background-color: ${ p => p.theme.colors.gray };
    border-radius: 1rem;
    border: 0px solid transparent;
  }
`;

const StatusBarWithRouter = withRouter(StatusBar);

const selLocal = selectLocalState(['toggleOmnibox']);
const selSettings = selectSettingsState(['display', 'getAll']);
const selGraph = (s: GraphState) => s.getShallowChildren;

const App: React.FunctionComponent = () => {
  const { getAll } = useSettingsState(selSettings, shallow);
  const { toggleOmnibox } = useLocalState(selLocal);
  const getShallowChildren = useGraphState(selGraph);
  const { theme, display } = useThemeWatcher();

  React.useEffect(() => {
    bootstrapApi();
    getShallowChildren(`~${window.ship}`, 'dm-inbox');

    getId().then((value) => {
      useLocalState.setState({ browserId: value });
    });

    getAll();
    gcpManager.start();
    Mousetrap.bindGlobal(['command+/', 'ctrl+/'], (e) => {
      e.preventDefault();
      e.stopImmediatePropagation();
      toggleOmnibox();
    });
  }, []);

  return (
    <ThemeProvider theme={theme}>
      <ShortcutContextProvider>
      <Helmet>
        {window.ship.length < 14
          ? <link rel="icon" type="image/svg+xml" href={svgDataURL(favicon())} />
          : null}
      </Helmet>
      <Root display={display}>
        <Router history={history}>
          <ErrorBoundary>
            <StatusBarWithRouter />
          </ErrorBoundary>
          <ErrorBoundary>
            <Omnibox />
          </ErrorBoundary>
          <ErrorBoundary>
            <Content />
          </ErrorBoundary>
        </Router>
      </Root>
      <div id="portal-root" />
      </ShortcutContextProvider>
    </ThemeProvider>
  );
};

export default App;

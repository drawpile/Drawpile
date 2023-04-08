// Drawdance's config file - https://github.com/drawpile/Drawdance
window.DRAWDANCE_CONFIG = {
  // Which WebSocket URL to connect to. The default is to just use the URL of
  // this page, with the protocol replaced by wss:// and /ws appended. You need
  // to point this at the URL where your Drawdance WebSocket proxy is running
  // and use the correct path as configured by the -ws-path option (or /ws by
  // default). The proxy probably needs to be TLS-secured, otherwise the browser
  // will refuse to connect. If you want to connect to a different server, the
  // proxy needs to be set to allow cross-origin requests.
  targetUrl: (function () {
    return `wss://${window.location.host}/ws`;
  })(),
  // The title for the initial dialog presented to the user.
  initialDialogTitle: `Join a Drawpile`,
  // The prompt of that same initial dialog asking the user to put in their
  // username. If you don't allow guest users, you can clarify that here.
  initialDialogPrompt: `
    Enter a username to join. If that user is registered, you'll be prompted for
    a password.
  `,
  // The stuff that gets put into the "Welcome" tab. You'll probably want to
  // customize this for your server to include rules and such in it. HTML is
  // supported here, so make sure you structure and escape it properly!
  welcomeMessage: `
    <h2>Welcome!</h2>
    <p>
      This is the default welcome message of Drawdance. If you're reading this,
      the person hosting this server hasn't changed it yet. They can do so by
      editing the <code>drawdance-config.js</code> file.
    </p>
    <p>
      Enter a username in the dialog on the left to join a Drawpile, a
      collaborative drawing session where multiple people can draw on the same
      canvas. This web client currently doesn't support drawing (yet?), only
      watching.
    </p>
    <p>
      Once you're connected, you can click and drag the canvas to move it
      around and use the mouse wheel to zoom in and out. On a mobile device, you
      can use your fingers to pan and pinch to zoom.
    </p>
    <p>
      This software is still deep in development, so there's bound to be bugs
      and other wonkiness. Apologies in advance. You can find Drawdance on
      GitHub at
      <a
        href="https://github.com/drawpile/Drawdance"
        target="_blank"
        rel="noreferrer"
      >
        github.com/drawpile/Drawdance/
      </a>
    </p>
  `,
};

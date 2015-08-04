import QtQuick 2.2

Item {
	property point position
	property string label
	property string label2
	property color cursorColor
	property bool showLabel2: true

	id: root

	x: position.x - width/2
	y: position.y - height
	width: 80
	height: 45

	onCursorColorChanged: canvas.requestPaint()
	onLabelChanged: canvas.recalcSize()
	onLabel2Changed: if(showLabel2) canvas.recalcSize()
	onShowLabel2Changed: canvas.recalcSize()

	Behavior on opacity {
		NumberAnimation { duration: 300 }
	}

	Canvas {
		property string font: "13px sans-serif"

		anchors.fill: parent
		id: canvas

		onPaint: {

			var MARGIN = 5;
			var PADDING = 3;
			var ROUND = 3;
			var ARROW = 10;

			console.log("repaint");
			var ctx = getContext("2d");

			ctx.font = font;
			ctx.clearRect(0, 0, width, height);

			// Draw the bubble
			var w = width - 2*MARGIN;
			var h = height - 2*MARGIN;

			ctx.save();
			ctx.translate(MARGIN, MARGIN);
			ctx.fillStyle = cursorColor;
			ctx.shadowBlur = 3;
			ctx.shadowColor = "#88000000";
			ctx.beginPath();

			ctx.moveTo(ROUND, 0);
			ctx.lineTo(w-ROUND, 0);
			ctx.quadraticCurveTo(w, 0, w, ROUND);

			ctx.lineTo(w, h-ROUND-ARROW);
			ctx.quadraticCurveTo(w, h-ARROW, w-ROUND, h-ARROW);

			ctx.lineTo(w/2 + ARROW, h-ARROW);
			ctx.lineTo(w/2, h);
			ctx.lineTo(w/2 - ARROW, h-ARROW);

			ctx.lineTo(ROUND, h-ARROW);

			ctx.quadraticCurveTo(0, h-ARROW, 0, h-ARROW-ROUND);
			ctx.lineTo(0, ROUND);
			ctx.quadraticCurveTo(0, 0, ROUND, 0);

			ctx.closePath();

			ctx.fill();
			ctx.restore();

			// Draw the label text
			var txtY = height-MARGIN-PADDING-ARROW-3;
			if(showLabel2) {
				ctx.fillText(label2, width/2 - ctx.measureText(label2).width/2, txtY);
				txtY -= 20;
			}
			ctx.fillText(label, width/2 - ctx.measureText(label).width/2, txtY);
		}

		function recalcSize() {
			var MARGIN = 5;
			var PADDING = 3;
			var ARROW = 10;

			var ctx = getContext("2d");
			ctx.font = font;

			var txtw  = ctx.measureText(label).width;
			if(showLabel2) {
				txtw = Math.max(txtw, ctx.measureText(label2).width);
			}

			root.width = Math.max(3*ARROW, txtw) + 2*MARGIN+2*PADDING;
			root.height = showLabel2 ? 60 : 45

			requestPaint();
		}
	}
}

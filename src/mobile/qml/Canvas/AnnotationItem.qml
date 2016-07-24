import QtQuick 2.2

Rectangle {
	property bool selected: false
	property real zoom: 1
	property var handles: []

	id: root
	clip: true
	border.color: "blue"
	border.width: selected ? 2 : 1
	color: background

	Text {
		anchors.fill: parent
		anchors.margins: 3
		text: display
		textFormat: Text.RichText
		wrapMode: Text.Wrap
	}

	onSelectedChanged: {
		if(selected) {
			var h = function(opts) { handles.push(draghandle.createObject(root, opts)); }
			h({"anchors.left": root.left, "anchors.top": root.top, "transformOrigin": Item.TopLeft });
			h({"anchors.left": root.left, "anchors.verticalCenter": root.verticalCenter, "transformOrigin": Item.Left});
			h({"anchors.left": root.left, "anchors.bottom": root.bottom, "transformOrigin": Item.BottomLeft});
			h({"anchors.horizontalCenter": root.horizontalCenter, "anchors.top": root.top, "transformOrigin": Item.Top});
			h({"anchors.horizontalCenter": root.horizontalCenter, "anchors.bottom": root.bottom, "transformOrigin": Item.Bottom});
			h({"anchors.right": root.right, "anchors.top": root.top, "transformOrigin": Item.TopRight});
			h({"anchors.right": root.right, "anchors.verticalCenter": root.verticalCenter, "transformOrigin": Item.Right});
			h({"anchors.right": root.right, "anchors.bottom": root.bottom, "transformOrigin": Item.BottomRight});

		} else {
			for(var i=0;i<handles.length;++i) {
				handles[i].destroy();
			}
			handles = [];
		}
	}

	Component {
		id: draghandle
		Rectangle {
			width: 10
			height: 10
			color: "blue"
			scale: root.zoom
		}
	}
}

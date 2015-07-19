import QtQuick 2.2
import QtQuick.Controls.Styles 1.4

ProgressBarStyle {
	background: Rectangle {
		border.color: "black"
		border.width: 1
		color: "#7f8c8d"

		implicitWidth: 200
		implicitHeight: 24
	}
	progress: Rectangle {
		color: "black"
		
		Rectangle {
			color: "#3daee9"
			anchors.top: parent.top
			anchors.bottom: parent.bottom
			anchors.topMargin: 1
			anchors.bottomMargin: 1
			width: parent.width > 3 ? 3: parent.width-1
			x: parent.width - width - 1
		}
	}
}


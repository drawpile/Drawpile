import QtQuick 2.2
import QtQuick.Controls 1.2
import QtQuick.Controls.Styles 1.2
import QtGraphicalEffects 1.0

Button {
	property int size: 32
	property string checkedIconName

	style: ButtonStyle {
		background: Item {
			implicitWidth: size
			implicitHeight: size
			Image {
				id: image
				anchors.fill: parent
				source: "image://theme/" + (control.checked ? checkedIconName : control.iconName)
				sourceSize.width: size
				sourceSize.height: size
				transform: Scale {
					origin.x: size/2
					origin.y: size/2
					xScale: control.pressed ? 0.9 : 1.0
					yScale: control.pressed ? 0.9 : 1.0
				}
				opacity: control.enabled ? 1.0 : 0.5
			}
			ColorOverlay {
				visible: control.hovered && !control.pressed
				anchors.fill: image
				source: image
				color: "#60ffffff"
			}
		}
	}
}


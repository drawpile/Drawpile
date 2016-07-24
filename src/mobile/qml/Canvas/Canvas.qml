import QtQuick 2.2
import Drawpile.Canvas 1.0

Rectangle {
	id: root
	color: "#646464"

	//Keys.onReleased: console.log("root key release", event.key);

	CanvasInputArea {
		id: canvasinput
		tablet: tabletState
		anchors.fill: parent

		canvas: canvas
		pressureMapping: inputConfig.pressureMapping

		Component.onCompleted: {
			canvasinput.penDown.connect(controller.startDrawing);
			canvasinput.penMove.connect(controller.continueDrawing);
			canvasinput.penUp.connect(controller.endDrawing);
		}

		LayerStack {
			id: canvas
			anchors.centerIn: parent

			transform: CanvasTransform {
				origin.x: canvas.width / 2
				origin.y: canvas.height / 2
				translateX: canvasinput.contentX
				translateY: canvasinput.contentY
				scale: canvasinput.contentZoom
				rotation: canvasinput.contentRotation
			}

			Repeater {
				id: annotations
				model: canvas.annotations
				AnnotationItem {
					x: rect.x
					y: rect.y
					width: rect.width
					height: rect.height
					zoom: 1/canvasinput.contentZoom
					selected: controller.activeAnnotation == annotationId
				}
			}

			Repeater {
				id: lasertrails
				PolyLine {
					anchors.fill: parent
					points: model.points
					lineWidth: 3
					lineStyle: PolyLine.LaserScan
					opacity: model.visible ? 1 : 0

					Behavior on opacity {
						NumberAnimation { duration: 1000 }
					}
				}
			}

			Repeater {
				id: usercursors
				UserCursor {
					position: model.pos
					label: model.display
					label2: '[' + model.layer + ']'
					cursorColor: model.color
					opacity: model.visible ? 1 : 0

					transformOrigin: Item.Bottom
					scale: 1/canvasinput.contentZoom
					rotation: -canvasinput.contentRotation
				}
			}
		}
	}

	Connections {
		target: controller
		onModelChanged: {
			canvas.model = controller.model.layerStack
			usercursors.model = controller.model.userCursors
			lasertrails.model = controller.model.laserTrails
		}
	}

	function initCanvas(model) {
		console.log("not doing anything!");
		console.log("toolctrl model:", controller.model);

	}
}


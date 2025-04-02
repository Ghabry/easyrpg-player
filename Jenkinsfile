pipeline {
    agent any

    environment {
        // Define environment variables if needed
    }

    stages {
        stage('Checkout') {
            steps {
                // Checkout the repository
                checkout scm
            }
        }

        stage('Configure') {
            steps {
                // Run CMake to configure the project
                sh 'cmake -S . -B build'
            }
        }

        stage('Build') {
            steps {
                // Run CMake to build the project
                sh 'cmake --build build'
            }
        }

        stage('Test') {
            steps {
                // Run CMake to execute tests
                sh 'cmake --build build --target check'
            }
        }

        stage('Package') {
            steps {
                // Run CPack to package the application
            }
        }
    }

    post {
        always {
            // Archive build artifacts and results
            archiveArtifacts artifacts: 'build/**/*', allowEmptyArchive: true
        }
        success {
            echo 'Build and test completed successfully.'
        }
        failure {
            echo 'Build or test failed.'
        }
    }
}
